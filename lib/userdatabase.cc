#include "userdatabase.hh"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QNetworkReply>
#include <QtConcurrent>
#include <algorithm>
#include "logger.hh"
#include <cmath>
#include "task_progress.hh"

namespace {
bool parseCsvRows(const QString &text, QList<QStringList> &rows, QString &error) {
  QStringList row;
  QString field;
  bool quoted=false;
  bool quoteClosed=false;
  auto finishRow=[&]() {
    row.append(field);
    field.clear();
    if (std::any_of(row.cbegin(), row.cend(), [](const QString &value) {
          return !value.trimmed().isEmpty();
        }))
      rows.append(row);
    row.clear();
    quoteClosed=false;
  };

  for (int index=0; index<text.size(); ++index) {
    const QChar character=text[index];
    if (quoted) {
      if ('"' == character) {
        if ((index+1 < text.size()) && ('"' == text[index+1])) {
          field.append('"');
          ++index;
        } else {
          quoted=false;
          quoteClosed=true;
        }
      } else {
        field.append(character);
      }
      continue;
    }

    if (quoteClosed && ((0x20 == character.unicode()) || (0x09 == character.unicode())))
      continue;
    if (quoteClosed && (',' != character) && ('\r' != character) && ('\n' != character)) {
      error=QObject::tr("Unexpected character after a quoted CSV field.");
      return false;
    }
    if (',' == character) {
      row.append(field);
      field.clear();
      quoteClosed=false;
    } else if ((0x0d == character.unicode()) || (0x0a == character.unicode())) {
      finishRow();
      if ((0x0d == character.unicode()) && (index+1 < text.size()) &&
          (0x0a == text[index+1].unicode()))
        ++index;
    } else if ('"' == character) {
      if (!field.isEmpty()) {
        error=QObject::tr("Unexpected quote in an unquoted CSV field.");
        return false;
      }
      quoted=true;
    } else {
      field.append(character);
    }
  }
  if (quoted) {
    error=QObject::tr("Unterminated quoted CSV field.");
    return false;
  }
  if (!field.isEmpty() || !row.isEmpty())
    finishRow();
  return true;
}

QString normalizeHeader(QString header) {
  if (!header.isEmpty() && (header[0] == QChar::ByteOrderMark))
    header.remove(0, 1);
  return header.trimmed().toLower();
}

bool parseRadioIdCSV(const QByteArray &data, QList<UserDatabase::User> &users, QString &error) {
  QList<QStringList> rows;
  if (!parseCsvRows(QString::fromUtf8(data), rows, error))
    return false;
  if (rows.isEmpty()) {
    error=QObject::tr("RadioID CSV is empty.");
    return false;
  }

  const QStringList header=rows.first();
  QHash<QString, int> columns;
  for (int index=0; index<header.size(); ++index)
    columns.insert(normalizeHeader(header[index]), index);
  auto column=[&](const QString &name) { return columns.value(name, -1); };
  const int idColumn=columns.contains(QStringLiteral("radio id"))
      ? column(QStringLiteral("radio id")) : column(QStringLiteral("dmr id"));
  const int callColumn=column(QStringLiteral("callsign"));
  if ((idColumn < 0) || (callColumn < 0)) {
    error=QObject::tr("RadioID CSV must contain 'Radio ID' and 'Callsign' columns.");
    return false;
  }

  users.clear();
  for (int rowIndex=1; rowIndex<rows.size(); ++rowIndex) {
    QStringList fields=rows[rowIndex];
    if (fields.size() > header.size()) {
      error=QObject::tr("RadioID CSV row %1 has more fields than the header.").arg(rowIndex+1);
      return false;
    }
    while (fields.size() < header.size())
      fields.append(QString());
    bool idValid=false;
    const uint id=fields[idColumn].trimmed().toUInt(&idValid);
    if (!idValid || (0 == id) || (id > 0xffffff)) {
      error=QObject::tr("RadioID CSV row %1 has an invalid 24-bit Radio ID.").arg(rowIndex+1);
      return false;
    }

    auto value=[&](const QString &name) {
      const int index=column(name);
      return (index < 0) ? QString() : fields[index].trimmed();
    };
    UserDatabase::User user;
    user.id=id;
    user.call=fields[callColumn].trimmed();
    user.name=value(QStringLiteral("name"));
    user.surname=value(QStringLiteral("nickname"));
    user.city=value(QStringLiteral("city"));
    user.state=value(QStringLiteral("state"));
    user.country=value(QStringLiteral("country"));
    user.comment=value(QStringLiteral("remarks"));
    users.append(user);
  }
  if (users.isEmpty()) {
    error=QObject::tr("RadioID CSV does not contain any user records.");
    return false;
  }
  return true;
}
}


/* ********************************************************************************************* *
 * Implementation of User
 * ********************************************************************************************* */
UserDatabase::User::User()
  : id(0)
{
  // pass...
}

UserDatabase::User::User(const QJsonObject &obj)
  : id(obj.value("id").toInt()), call(obj.value("callsign").toString()),
    name(obj.value("fname").toString()), surname(obj.value("surname").toString()),
    city(obj.value("city").toString()), state(obj.value("state").toString()),
    country(obj.value("country").toString()), comment(obj.value("remarks").toString())
{
  // pass...
}

unsigned
UserDatabase::User::distance(unsigned id) const {
  // Fix number of digits
  int a = this->id, b = id;
  int ad = std::ceil(std::log10(a));
  int bd = std::ceil(std::log10(b));
  if (ad > bd)
    b *= std::pow(10u, (ad-bd));
  else if (bd > ad)
    a *= std::pow(10u, (bd-ad));
  // Distance is just the difference between these two numbers
  // this ensures a small distance between two numbers with the same
  // prefix.
  return std::abs(a-b);
}


/* ********************************************************************************************* *
 * Implementation of UserDatabase
 * ********************************************************************************************* */
UserDatabase::UserDatabase(bool parallel, unsigned updatePeriodDays, QObject *parent)
  : QAbstractTableModel(parent), _user(), _network()
{
  connect(&_network, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(downloadFinished(QNetworkReply*)));

  if ((! exists()) || (updatePeriodDays < dbAge()))
    download();
  else {
    if (parallel) {
      _parsing = QtConcurrent::run([this]() { QList<User> users; this->parse(users); return users;})
                   .then(this, [this](const QList<User> &users){ return this->load(users); });
    } else {
      load();
    }
  }
}

qint64
UserDatabase::count() const {
  return _user.size();
}

bool
UserDatabase::exists() const {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFileInfo info(path + "/user.json");
  return info.isFile() && info.isReadable();
}

bool
UserDatabase::ready() const {
  return ! _user.empty();
}

bool
UserDatabase::load() {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return load(path+"/user.json");
}

const UserDatabase::User &
UserDatabase::user(int idx) const {
  return _user[idx];
}

bool
UserDatabase::load(const QString &filename) {
  QList<User> users;
  if (! parse(filename, users))
    return false;

  auto res =  load(users);
  logDebug() << "Loaded user database with " << _user.size() << " entries from " << filename << ".";
  return res;
}

bool
UserDatabase::load(const QList<User> &users) {
  beginResetModel();
  _user.clear(); emit readyChanged(false);
  _user.reserve(users.size());
  for (auto user: users)
    _user.append(user);
  endResetModel();

  if (ready())
    emit readyChanged(ready());
  emit loaded();
  return true;
}


bool
UserDatabase::parse(QList<User> &users) {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return parse(path+"/user.json", users);
}

bool
UserDatabase::parse(const QString &filename, QList<User> &users)
{
  QFile file(filename);
  if (! file.open(QIODevice::ReadOnly)) {
    QString msg = QString("Cannot open user list '%1': %2").arg(filename).arg(file.errorString());
    logError() << msg;
    emit error(msg);
    return false;
  }
  QByteArray data = file.readAll();
  file.close();

  if (QStringLiteral("csv").compare(QFileInfo(filename).suffix(), Qt::CaseInsensitive) == 0) {
    QString errorMessage;
    if (!parseRadioIdCSV(data, users, errorMessage)) {
      const QString msg=QString("Failed to load call-sign CSV: %1").arg(errorMessage);
      logError() << msg;
      emit error(msg);
      return false;
    }
    logDebug() << "Loaded user database with " << users.size() << " entries from " << filename << ".";
    return true;
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(data, &err);
  if (doc.isEmpty()) {
    QString msg = "Failed to load user DB: " + err.errorString();
    logError() << msg;
    emit error(msg);
    return false;
  }

  if (! doc.isObject()) {
    QString msg = "Failed to load user DB: JSON document is not an object!";
    logError() << msg;
    emit error(msg);
    return false;
  }
  if (! doc.object().contains("users")) {
    QString msg = "Failed to load user DB: JSON object does not contain 'users' item.";
    logError() << msg;
    emit error(msg);
    return false;
  }
  if (! doc.object()["users"].isArray()) {
    QString msg = "Failed to load user DB: 'users' item is not an array.";
    logError() << msg;
    emit error(msg);
    return false;
  }

  users.clear();
  QJsonArray array = doc.object()["users"].toArray();
  users.reserve(array.size());
  for (int i=0; i<array.size(); i++) {
    User user(array.at(i).toObject());
    if (user.isValid())
      users.append(user);
  }
  // Sort repeater w.r.t. their IDs
  std::stable_sort(_user.begin(), _user.end(), [](const User &a, const User &b){ return a.id < b.id; });

  return true;
}

void
UserDatabase::sortUsers(unsigned id) {
  // Sort repeater w.r.t. distance to ID
  std::stable_sort(_user.begin(), _user.end(), [id](const User &a, const User &b){
    return a.distance(id) < b.distance(id);
  });
}

void
UserDatabase::sortUsers(const QSet<unsigned> &ids) {
  if (0 == ids.count())
    return;

  // Sort repeater w.r.t. distance to each ID
  std::stable_sort(_user.begin(), _user.end(), [ids](const User &a, const User &b){
    QSet<unsigned>::const_iterator id=ids.begin();
    unsigned min_a = a.distance(*id), min_b = b.distance(*id);
    id++;
    for (; id!=ids.end(); id++) {
      min_a = std::min(min_a, a.distance(*id));
      min_b = std::min(min_b, b.distance(*id));
    }
    return min_a < min_b;
  });
}

TaskProgress *
UserDatabase::download() {
  QUrl url("https://database.radioid.net/static/users.json");
  QNetworkRequest request(url);
  auto reply = _network.get(request);
  return new DownloadTaskProgress(tr("Download call-sign db"), reply);
}

void
UserDatabase::downloadFinished(QNetworkReply *reply) {
  if (reply->error()) {
    QString msg = QString("Cannot download user database: %1").arg(reply->errorString());
    logError() << msg;
    emit error(msg);
    return;
  }

  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QFile file(path+"/user.json");
  QDir directory;
  if ((! directory.exists(path)) && (!directory.mkpath(path))) {
    QString msg = QString("Cannot create path '%1'.").arg(path);
    logError() << msg;
    emit error(msg);
    return;
  }
  if (! file.open(QIODevice::WriteOnly)) {
    QString msg = QString("Cannot save user database at '%1'.").arg(path+"/user.json");
    logError() << msg;
    emit error(msg);
    return;
  }

  file.write(reply->readAll());
  file.flush();
  file.close();

  load();
  reply->deleteLater();
}

unsigned
UserDatabase::dbAge() const {
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/user.json";
  QFileInfo info(path);
  if (! info.exists())
    return -1;
  return info.lastModified().daysTo(QDateTime::currentDateTime());
}

int
UserDatabase::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return _user.size();
}

int
UserDatabase::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 3;
}

QVariant
UserDatabase::data(const QModelIndex &index, int role) const {
  if ((Qt::EditRole != role) && ((Qt::DisplayRole != role)))
    return QVariant();

  if (index.row() >= _user.size())
    return QVariant();

  if (0 == index.column()) {
    // Call
    if (Qt::DisplayRole == role) {
      if (_user[index.row()].surname.isEmpty()) {
        if (_user[index.row()].name.isEmpty()) {
          return _user[index.row()].call;
        } else {
          return tr("%1 (%2)")
              .arg(_user[index.row()].call)
              .arg(_user[index.row()].name);
        }
      } else {
        return tr("%1 (%2, %3)")
            .arg(_user[index.row()].call)
            .arg(_user[index.row()].name)
            .arg(_user[index.row()].surname);
      }
    } else {
      return _user[index.row()].call;
    }
  } else if (1 == index.column()) {
    // ID
    return _user[index.row()].id;
  } else if (2 == index.column()) {
    // Country
    return _user[index.column()].country;
  }

  return QVariant();
}

