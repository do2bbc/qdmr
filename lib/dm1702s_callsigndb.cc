#include "dm1702s_callsigndb.hh"

#include "logger.hh"

#include <algorithm>
#include <cstring>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr unsigned int sectorSize = 0x1000;
constexpr unsigned int entrySize = 108;
constexpr unsigned int entriesInFirstPage = 37;
constexpr unsigned int entriesPerPage = 37;

unsigned int entryOffset(unsigned int index) {
  const unsigned int page = index < entriesInFirstPage
      ? 0 : 1+(index-entriesInFirstPage)/entriesPerPage;
  const unsigned int entry = index < entriesInFirstPage
      ? index : (index-entriesInFirstPage)%entriesPerPage;
  return page*sectorSize + (0 == page ? 16 : 0) + entry*entrySize;
}

QString readField(const QByteArray &data, unsigned int offset, unsigned int size) {
  unsigned int length=0;
  while ((length < size) && (data[offset+length] != '\0') &&
         (static_cast<unsigned char>(data[offset+length]) != 0xff))
    ++length;
  return QString::fromLatin1(data.constData()+offset, static_cast<int>(length));
}

bool writeField(QByteArray &data, unsigned int offset, unsigned int size,
                const QString &value, const QString &field, const ErrorStack &err) {
  const QByteArray encoded=value.toLatin1();
  if (QString::fromLatin1(encoded) != value) {
    errMsg(err) << "DM1702S call-sign database " << field << " is not Latin-1 encodable.";
    return false;
  }
  const int length=std::min(static_cast<int>(size), static_cast<int>(encoded.size()));
  if (length)
    std::memcpy(data.data()+offset, encoded.constData(), static_cast<size_t>(length));
  return true;
}
}

bool
DM1702SCallsignDB::decodeToJson(const QByteArray &data, QByteArray &json,
                                const ErrorStack &err) {
  if (data.size() != static_cast<int>(imageSize())) {
    errMsg(err) << "Invalid DM1702S call-sign database image size.";
    return false;
  }
  const unsigned int count=static_cast<uint8_t>(data[0]) |
      (static_cast<unsigned int>(static_cast<uint8_t>(data[1])) << 8);
  if (count > maxEntries()) {
    errMsg(err) << "Invalid DM1702S call-sign database entry count " << count << ".";
    return false;
  }

  QJsonArray users;
  for (unsigned int index=0; index<count; ++index) {
    const unsigned int offset=entryOffset(index);
    const uint8_t type=static_cast<uint8_t>(data[offset]);
    const unsigned int id=static_cast<uint8_t>(data[offset+1]) |
        (static_cast<unsigned int>(static_cast<uint8_t>(data[offset+2])) << 8) |
        (static_cast<unsigned int>(static_cast<uint8_t>(data[offset+3])) << 16);
    if ((0x03 != type) || (0 == id)) {
      errMsg(err) << "Invalid DM1702S call-sign database entry " << (index+1) << ".";
      return false;
    }

    QJsonObject user;
    user.insert(QStringLiteral("id"), static_cast<int>(id));
    user.insert(QStringLiteral("callsign"), readField(data, offset+4, 8));
    user.insert(QStringLiteral("fname"), readField(data, offset+12, 16));
    user.insert(QStringLiteral("surname"), readField(data, offset+28, 16));
    user.insert(QStringLiteral("city"), readField(data, offset+44, 16));
    user.insert(QStringLiteral("state"), readField(data, offset+60, 16));
    user.insert(QStringLiteral("country"), readField(data, offset+76, 16));
    user.insert(QStringLiteral("remarks"), readField(data, offset+92, 16));
    users.append(user);
  }

  QJsonObject root;
  root.insert(QStringLiteral("users"), users);
  json=QJsonDocument(root).toJson(QJsonDocument::Indented);
  return true;
}

bool
DM1702SCallsignDB::encode(const QVector<UserDatabase::User> &users, QByteArray &data,
                          const ErrorStack &err) {
  if (users.isEmpty() || (users.size() > static_cast<int>(maxEntries()))) {
    errMsg(err) << "DM1702S call-sign database requires 1 to " << maxEntries() << " entries.";
    return false;
  }

  data=QByteArray(static_cast<int>(imageSize()), '\0');
  const unsigned int count=static_cast<unsigned int>(users.size());
  data[0]=static_cast<char>(count & 0xff);
  data[1]=static_cast<char>((count >> 8) & 0xff);

  for (unsigned int index=0; index<count; ++index) {
    const UserDatabase::User &user=users[static_cast<int>(index)];
    if ((0 == user.id) || (user.id > 0xffffff)) {
      errMsg(err) << "Invalid DMR ID in DM1702S call-sign database entry " << (index+1) << ".";
      return false;
    }

    const unsigned int offset=entryOffset(index);
    data[offset]=0x03;
    data[offset+1]=static_cast<char>(user.id & 0xff);
    data[offset+2]=static_cast<char>((user.id >> 8) & 0xff);
    data[offset+3]=static_cast<char>((user.id >> 16) & 0xff);
    if (!writeField(data, offset+4, 8, user.call, QStringLiteral("callsign"), err) ||
        !writeField(data, offset+12, 16, user.name, QStringLiteral("name"), err) ||
        !writeField(data, offset+28, 16, user.surname, QStringLiteral("nickname"), err) ||
        !writeField(data, offset+44, 16, user.city, QStringLiteral("city"), err) ||
        !writeField(data, offset+60, 16, user.state, QStringLiteral("state"), err) ||
        !writeField(data, offset+76, 16, user.country, QStringLiteral("country"), err) ||
        !writeField(data, offset+92, 16, user.comment, QStringLiteral("remarks"), err))
      return false;
  }

  return true;
}