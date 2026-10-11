#include "dm1702s.hh"

#include "dm1702s_callsigndb.hh"
#include "dm1702s_interface.hh"
#include "logger.hh"
#include "radiolimits.hh"

#include <algorithm>
#include <QMap>
#include <QSet>

namespace {
class DM1702SRadioLimits final : public RadioLimits {
public:
  DM1702SRadioLimits() : RadioLimits(true) {
    _hasCallSignDB = true;
    _callSignDBImplemented = true;
    _numCallSignDBEntries = DM1702SCallsignDB::maxEntries();
  }
};

constexpr uint8_t pageReadOrder[] = {
  0x02, 0x16, 0x24, 0x04, 0x45, 0x7c, 0x11, 0x03, 0x7b, 0x13, 0x12, 0x06,
  0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22,
  0x23, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54,
  0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
  0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x3f
};
}

DM1702S::DM1702S(DM1702SInterface *device, QObject *parent)
  : Radio(parent), _device(device), _name(QStringLiteral("Baofeng DM-1702S")), _codeplug(),
    _config(nullptr), _codeplugFlags()
{
}

RadioLimits *DM1702S::_limits = nullptr;

DM1702S::~DM1702S() {
  if (_device && _device->isOpen())
    _device->close();
  if (_device) {
    _device->deleteLater();
    _device = nullptr;
  }
}

RadioInfo
DM1702S::defaultRadioInfo() {
  return RadioInfo::byID(RadioInfo::DM1702S);
}

const QString &
DM1702S::name() const {
  return _name;
}

const RadioLimits &
DM1702S::limits() const {
  if (nullptr == _limits)
    _limits = new DM1702SRadioLimits();
  return *_limits;
}

const Codeplug &
DM1702S::codeplug() const {
  return _codeplug;
}

Codeplug &
DM1702S::codeplug() {
  return _codeplug;
}

bool
DM1702S::startDownload(const TransferFlags &flags, const ErrorStack &err) {
  if ((StatusIdle != _task) || (nullptr == _device))
    return false;
  _task = StatusDownload;
  _errorStack = err;
  if (flags.blocking()) {
    run();
    return StatusIdle == _task;
  }
  if (_device->isOpen())
    _device->moveToThread(this);
  start();
  return true;
}

bool
DM1702S::startDownloadCallsignDB(const TransferFlags &flags, const ErrorStack &err) {
  if ((StatusIdle != _task) || (nullptr == _device))
    return false;
  _task = StatusDownloadCallsigns;
  _errorStack = err;
  if (flags.blocking()) {
    run();
    return StatusIdle == _task;
  }
  if (_device->isOpen())
    _device->moveToThread(this);
  start();
  return true;
}

bool
DM1702S::startUpload(Config *config, const Codeplug::Flags &flags, const ErrorStack &err) {
  if ((StatusIdle != _task) || (nullptr == _device) || (nullptr == config))
    return false;
  _config = config;
  _codeplugFlags = flags;
  _task = StatusUpload;
  _errorStack = err;
  if (flags.blocking()) {
    run();
    return StatusIdle == _task;
  }
  if (_device->isOpen())
    _device->moveToThread(this);
  start();
  return true;
}

bool
DM1702S::startUploadCallsignDB(UserDatabase *db, const CallsignDB::Flags &selection,
                               const ErrorStack &err) {
  if ((StatusIdle != _task) || (nullptr == _device) || (nullptr == db))
    return false;

  unsigned int count=static_cast<unsigned int>(std::min<qint64>(
      DM1702SCallsignDB::maxEntries(), db->count()));
  if (selection.hasCountLimit())
    count=static_cast<unsigned int>(std::min<size_t>(count, selection.countLimit()));
  QVector<UserDatabase::User> users;
  users.reserve(static_cast<int>(count));
  for (unsigned int index=0; index<count; ++index)
    users.append(db->user(static_cast<int>(index)));
  if (!DM1702SCallsignDB::encode(users, _callSignDatabase, err))
    return false;

  _task = StatusUploadCallsigns;
  _errorStack = err;
  if (selection.blocking()) {
    run();
    return StatusIdle == _task;
  }
  if (_device->isOpen())
    _device->moveToThread(this);
  start();
  return true;
}

bool
DM1702S::startUploadSatelliteConfig(SatelliteDatabase *db, const TransferFlags &flags,
                                    const ErrorStack &err) {
  Q_UNUSED(db)
  Q_UNUSED(flags)
  errMsg(err) << "DM1702S satellite configuration upload is not implemented.";
  return false;
}

bool
DM1702S::download(const ErrorStack &err) {
  _codeplug.clearPages();
  if (!_device->read_start(0, 0, err)) {
    errMsg(err) << "Cannot enter DM1702S codeplug read mode.";
    return false;
  }

  const auto range = _device->memoryRange();
  const uint32_t firstPage = range.first & ~0xfffu;
  const uint32_t lastPage = range.second & ~0xfffu;
  if ((range.first > range.second) || (lastPage-firstPage > 0x100000)) {
    errMsg(err) << "Invalid DM1702S codeplug memory range.";
    return false;
  }

  const uint32_t pageCount = ((lastPage-firstPage)/0x1000)+1;
  QMap<uint8_t, uint32_t> pageAddresses;
  for (uint32_t address=firstPage; address<=lastPage; address+=0x1000) {
    const uint32_t pageNumber = ((address-firstPage)/0x1000)+1;
    emit downloadProgress((50*pageNumber)/pageCount);

    uint8_t marker = 0;
    if (!_device->read(0, address+0xfff, &marker, 1, err))
      return false;
    if ((0 == marker) || (0xff == marker))
      continue;
    if (pageAddresses.contains(marker)) {
      errMsg(err) << "Duplicate DM1702S page marker " << Qt::hex << marker << ".";
      return false;
    }
    pageAddresses.insert(marker, address);
  }

  if (pageAddresses.isEmpty()) {
    errMsg(err) << "No DM1702S codeplug pages were found.";
    return false;
  }

  uint32_t pageIndex = 0;
  QSet<uint8_t> readMarkers;
  auto readPage = [&](uint8_t marker, uint32_t address) {
    emit downloadProgress(50 + (50*++pageIndex)/pageAddresses.size());
    QByteArray page(0x1000, '\0');
    if (!_device->read(0, address, reinterpret_cast<uint8_t *>(page.data()),
                       page.size(), err))
      return false;
    if (static_cast<uint8_t>(page[0xfff]) != marker) {
      errMsg(err) << "DM1702S page marker changed while reading address " << Qt::hex << address << ".";
      return false;
    }
    if (!_codeplug.setPageAtAddress(marker, page, address, err))
      return false;
    readMarkers.insert(marker);
    return true;
  };

  for (uint8_t marker: pageReadOrder) {
    auto address = pageAddresses.constFind(marker);
    if ((address != pageAddresses.cend()) && !readPage(marker, address.value()))
      return false;
  }
  for (auto pageAddress=pageAddresses.cbegin(); pageAddress!=pageAddresses.cend(); ++pageAddress) {
    if (!readMarkers.contains(pageAddress.key()) &&
        !readPage(pageAddress.key(), pageAddress.value()))
      return false;
  }

  return _device->read_finish(err);
}

bool
DM1702S::downloadCallsignDatabase(const ErrorStack &err) {
  if (!_device->read_start(0, 0, err)) {
    errMsg(err) << "Cannot enter DM1702S call-sign database read mode.";
    return false;
  }

  const uint32_t address=DM1702SCallsignDB::address();
  const uint32_t lastAddress=address+DM1702SCallsignDB::imageSize()-1;
  if (lastAddress > 0xffffff) {
    errMsg(err) << "DM1702S call-sign database exceeds the 24-bit address space.";
    return false;
  }

  QByteArray image(static_cast<int>(DM1702SCallsignDB::imageSize()), '\0');
  constexpr int chunkSize=0x1000;
  for (int offset=0; offset<image.size(); offset+=chunkSize) {
    const int length=std::min(chunkSize, static_cast<int>(image.size())-offset);
    if (!_device->read(0, address+offset,
                       reinterpret_cast<uint8_t *>(image.data()+offset), length, err)) {
      errMsg(err) << "Cannot read DM1702S call-sign database at "
                  << Qt::hex << (address+offset) << ".";
      return false;
    }
    emit downloadProgress((99*(offset+length))/image.size());
  }
  if (!_device->read_finish(err)) {
    errMsg(err) << "Cannot finish DM1702S call-sign database read.";
    return false;
  }
  if (!DM1702SCallsignDB::decodeToJson(image, _downloadedCallsignDatabaseJson, err))
    return false;
  emit downloadProgress(100);
  return true;
}

bool
DM1702S::upload(const ErrorStack &err) {
  if (!download(err)) {
    errMsg(err) << "Cannot preserve existing DM1702S codeplug before upload.";
    return false;
  }
  Config currentConfig;
  if (!_codeplug.decode(&currentConfig, err)) {
    errMsg(err) << "Cannot decode the current DM1702S codeplug before upload.";
    return false;
  }
  if (!_codeplug.encode(_config, _codeplugFlags, err)) {
    errMsg(err) << "Cannot encode DM1702S configuration.";
    return false;
  }
  if (!_device->write_start(0, 0, err)) {
    errMsg(err) << "Cannot start DM1702S codeplug upload.";
    return false;
  }

  QSet<uint8_t> writtenMarkers;
  uint32_t pageIndex = 0;
  uint32_t pagesToWrite = 0;
  for (uint8_t marker: _codeplug.pageMarkers())
    if (marker != 0x02)
      ++pagesToWrite;
  auto writePage = [&](uint8_t marker) {
    if ((marker == 0x02) || writtenMarkers.contains(marker))
      return true;
    const QByteArray data = _codeplug.page(marker);
    if (data.isEmpty())
      return true;
    const uint32_t address = _codeplug.pageAddress(marker);
    if (std::numeric_limits<uint32_t>::max() == address) {
      errMsg(err) << "No physical address is known for DM1702S page " << Qt::hex << marker << ".";
      return false;
    }
    emit uploadProgress((100*++pageIndex)/pagesToWrite);
    if (!_device->write(0, address, reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
                        data.size(), err))
      return false;
    writtenMarkers.insert(marker);
    return true;
  };

  for (uint8_t marker: pageReadOrder)
    if (!writePage(marker))
      return false;
  for (uint8_t marker: _codeplug.pageMarkers())
    if (!writePage(marker))
      return false;

  return _device->write_finish(err);
}

bool
DM1702S::uploadCallsignDatabase(const ErrorStack &err) {
  if (!_device->read_start(0, 0, err)) {
    errMsg(err) << "Cannot enter DM1702S call-sign database write mode.";
    return false;
  }
  if (!_device->write_start(0, 0, err))
    return false;
  if (!_device->write(0, DM1702SCallsignDB::address(),
                      reinterpret_cast<uint8_t *>(_callSignDatabase.data()),
                      _callSignDatabase.size(), err)) {
    errMsg(err) << "Cannot write DM1702S call-sign database.";
    return false;
  }
  emit uploadProgress(100);
  return _device->write_finish(err);
}

void
DM1702S::run() {
  if ((StatusDownload != _task) && (StatusUpload != _task) &&
  (StatusDownloadCallsigns != _task) && (StatusUploadCallsigns != _task))
    return;
  const Status task=_task;
  if (!_device || !_device->isOpen()) {
    _task = StatusError;
    if ((StatusDownload == task) || (StatusDownloadCallsigns == task))
      emit downloadError(this);
    else
      emit uploadError(this);
    return;
  }

  if (StatusDownload == task) {
    emit downloadStarted();
    if (!download(_errorStack)) {
      _device->read_finish();
      _device->close();
      _task = StatusError;
      emit downloadError(this);
      return;
    }
    _task = StatusIdle;
    _device->close();
    emit downloadFinished(this, &codeplug());
  } else if (StatusDownloadCallsigns == task) {
    emit downloadStarted();
    if (!downloadCallsignDatabase(_errorStack)) {
      _device->read_finish();
      _device->close();
      _task = StatusError;
      emit downloadError(this);
      return;
    }
    _task = StatusIdle;
    _device->close();
    emit downloadCallsignDBFinished(this, _downloadedCallsignDatabaseJson);
  } else if (StatusUpload == task) {
    emit uploadStarted();
    if (!upload(_errorStack)) {
      _device->write_finish();
      _device->close();
      _task = StatusError;
      emit uploadError(this);
      return;
    }
    _task = StatusIdle;
    _device->close();
    emit uploadComplete(this);
  } else {
    emit uploadStarted();
    if (!uploadCallsignDatabase(_errorStack)) {
      _device->write_finish();
      _device->close();
      _task = StatusError;
      emit uploadError(this);
      return;
    }
    _task = StatusIdle;
    _device->close();
    emit uploadComplete(this);
  }
}