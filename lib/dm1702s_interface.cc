#include "dm1702s_interface.hh"

#include "logger.hh"

#include <algorithm>
#include <cstring>

namespace {
constexpr uint16_t vendorId = 0x0483;
constexpr uint16_t productId = 0x5780;
constexpr int interfaceNumber = 0;
constexpr unsigned char endpointOut = 0x01;
constexpr unsigned char endpointIn = 0x82;
constexpr unsigned int timeoutMs = 1000;
constexpr int maxReadPayload = 506;

uint32_t littleEndian32(const QByteArray &data, int offset) {
  return static_cast<uint8_t>(data[offset]) |
      (static_cast<uint32_t>(static_cast<uint8_t>(data[offset+1])) << 8) |
      (static_cast<uint32_t>(static_cast<uint8_t>(data[offset+2])) << 16) |
      (static_cast<uint32_t>(static_cast<uint8_t>(data[offset+3])) << 24);
}
}

DM1702SInterface::Descriptor::Descriptor(const USBDeviceInfo &info, uint8_t bus, uint8_t device)
  : USBDeviceDescriptor(info, USBDeviceHandle(bus, device))
{
}

DM1702SInterface::DM1702SInterface(const USBDeviceDescriptor &descr,
                                   const ErrorStack &err, QObject *parent)
  : QObject(parent), _context(nullptr), _device(nullptr), _detachedKernelDriver(false),
    _identified(false), _programMode(false), _radioInfo(), _firmware(), _memoryRange()
{
  if (interfaceInfo() != descr) {
    errMsg(err) << "Cannot connect to DM1702S using " << descr.description() << ".";
    return;
  }

  int result = libusb_init(&_context);
  if (result < 0) {
    errMsg(err) << "Cannot initialize libusb: " << libusb_error_name(result) << ".";
    _context = nullptr;
    return;
  }

  libusb_device **devices = nullptr;
  const ssize_t count = libusb_get_device_list(_context, &devices);
  if (count < 0) {
    errMsg(err) << "Cannot enumerate USB devices: " << libusb_error_name(static_cast<int>(count)) << ".";
    close();
    return;
  }

  const USBDeviceHandle address = descr.device().value<USBDeviceHandle>();
  libusb_device *matchingDevice = nullptr;
  for (ssize_t index=0; index<count; ++index) {
    libusb_device_descriptor usbDescr{};
    if (libusb_get_device_descriptor(devices[index], &usbDescr) < 0)
      continue;
    if ((usbDescr.idVendor == vendorId) && (usbDescr.idProduct == productId) &&
        (libusb_get_bus_number(devices[index]) == address.bus) &&
        (libusb_get_device_address(devices[index]) == address.device)) {
      matchingDevice = devices[index];
      break;
    }
  }

  if (matchingDevice)
    result = libusb_open(matchingDevice, &_device);
  libusb_free_device_list(devices, 1);
  if (!matchingDevice || (result < 0)) {
    errMsg(err) << "Cannot open DM1702S USB device.";
    close();
    return;
  }

  if (1 == libusb_kernel_driver_active(_device, interfaceNumber)) {
    result = libusb_detach_kernel_driver(_device, interfaceNumber);
    if (result < 0) {
      errMsg(err) << "Cannot detach USB kernel driver: " << libusb_error_name(result) << ".";
      close();
      return;
    }
    _detachedKernelDriver = true;
  }

  result = libusb_claim_interface(_device, interfaceNumber);
  if (result < 0) {
    errMsg(err) << "Cannot claim DM1702S USB interface: " << libusb_error_name(result) << ".";
    close();
  }
}

DM1702SInterface::~DM1702SInterface() {
  close();
}

bool
DM1702SInterface::isOpen() const {
  return nullptr != _device;
}

void
DM1702SInterface::close() {
  if (_device) {
    libusb_release_interface(_device, interfaceNumber);
    if (_detachedKernelDriver)
      libusb_attach_kernel_driver(_device, interfaceNumber);
    libusb_close(_device);
  }
  if (_context)
    libusb_exit(_context);
  _device = nullptr;
  _context = nullptr;
  _detachedKernelDriver = false;
  _programMode = false;
}

bool
DM1702SInterface::send(const QByteArray &data, const ErrorStack &err) {
  if (!isOpen() || data.isEmpty())
    return false;
  int transferred = 0;
  const int result = libusb_bulk_transfer(_device, endpointOut,
      reinterpret_cast<unsigned char *>(const_cast<char *>(data.constData())), data.size(),
      &transferred, timeoutMs);
  if ((result < 0) || (transferred != data.size())) {
    errMsg(err) << "DM1702S USB write failed: " << libusb_error_name(result) << ".";
    return false;
  }
  return true;
}

bool
DM1702SInterface::receiveExact(int length, QByteArray &data, const ErrorStack &err) {
  if (!isOpen() || (length <= 0))
    return false;
  data.clear();
  data.reserve(length);
  unsigned int stallRetries = 0;
  while (data.size() < length) {
    QByteArray fragment(length-data.size(), '\0');
    int transferred = 0;
    const int result = libusb_bulk_transfer(_device, endpointIn,
        reinterpret_cast<unsigned char *>(fragment.data()), fragment.size(),
        &transferred, timeoutMs);
    if (transferred > 0) {
      fragment.resize(transferred);
      data.append(fragment);
    }
    if (result < 0) {
      if ((LIBUSB_ERROR_PIPE == result) && (stallRetries++ < 2) &&
          (libusb_clear_halt(_device, endpointIn) >= 0))
        continue;
      errMsg(err) << "DM1702S USB read failed: " << libusb_error_name(result)
                  << " after " << data.size() << '/' << length << " bytes.";
      return false;
    }
    if (transferred <= 0) {
      errMsg(err) << "DM1702S USB read returned no data after "
                  << data.size() << '/' << length << " bytes.";
      return false;
    }
    stallRetries = 0;
  }
  return true;
}

bool
DM1702SInterface::receiveAvailable(int capacity, QByteArray &data, const char *operation,
                                   const ErrorStack &err) {
  if (!isOpen() || (capacity <= 0))
    return false;
  data.resize(capacity);
  unsigned int retries = 0;
  while (true) {
    int transferred = 0;
    const int result = libusb_bulk_transfer(_device, endpointIn,
        reinterpret_cast<unsigned char *>(data.data()), capacity, &transferred, timeoutMs);
    if (transferred > 0) {
      data.resize(transferred);
      return true;
    }
    if (((LIBUSB_ERROR_PIPE == result) || (LIBUSB_ERROR_IO == result)) && (retries++ < 2) &&
        (libusb_clear_halt(_device, endpointIn) >= 0))
      continue;
    errMsg(err) << "DM1702S USB read for " << operation << " failed: "
                << libusb_error_name(result) << " without data.";
    return false;
  }
}

bool
DM1702SInterface::acknowledge(const ErrorStack &err) {
  if (!send(QByteArray(1, '\x06'), err))
    return false;
  QByteArray response;
  if (!receiveExact(1, response, err) || (response[0] != '\x06')) {
    errMsg(err) << "Unexpected DM1702S acknowledgement.";
    return false;
  }
  return true;
}

bool
DM1702SInterface::readValue(uint8_t id, QByteArray &value, const ErrorStack &err) {
  QByteArray request(5, '\0');
  request[0] = 'V';
  request[4] = static_cast<char>(id);
  QByteArray header;
  if (!send(request, err) || !receiveExact(3, header, err))
    return false;
  if ((header[0] != 'V') || (static_cast<uint8_t>(header[1]) != id)) {
    errMsg(err) << "Unexpected DM1702S value response.";
    return false;
  }
  if (!receiveExact(static_cast<uint8_t>(header[2]), value, err) || !acknowledge(err))
    return false;
  return true;
}

bool
DM1702SInterface::readConfigBlock(uint16_t address, uint8_t length, QByteArray &data,
                                  const ErrorStack &err) {
  QByteArray request(5, '\0');
  request[0] = 'V';
  request[1] = static_cast<char>(address & 0xff);
  request[2] = static_cast<char>((address >> 8) & 0xff);
  request[3] = static_cast<char>(length);
  request[4] = 0x0d;
  QByteArray header;
  if (!send(request, err) || !receiveExact(3, header, err))
    return false;
  if ((header[0] != 'V') || (static_cast<uint8_t>(header[1]) != 0x0d) ||
      (static_cast<uint8_t>(header[2]) != length)) {
    errMsg(err) << "Unexpected DM1702S configuration-block response.";
    return false;
  }
  return receiveExact(length, data, err) && acknowledge(err);
}

RadioInfo
DM1702SInterface::identifier(const ErrorStack &err) {
  if (_identified)
    return _radioInfo;
  if (!isOpen())
    return RadioInfo();

  QByteArray response;
  if (!send(QByteArray("PSEARCH", 7), err) || !receiveExact(8, response, err) ||
      (static_cast<uint8_t>(response[0]) != 0x06) || (response.mid(1) != "DM1702S")) {
    errMsg(err) << "The USB device did not identify as DM1702S.";
    return RadioInfo();
  }

  if (!send(QByteArray("PASSSTA", 7), err) || !receiveExact(3, response, err) ||
      (response[0] != 'P')) {
    errMsg(err) << "DM1702S programming handshake failed.";
    return RadioInfo();
  }
  if (!send(QByteArray("SYSINFO", 7), err) || !receiveExact(1, response, err) ||
      (static_cast<uint8_t>(response[0]) != 0x06)) {
    errMsg(err) << "Cannot enter DM1702S system-info mode.";
    return RadioInfo();
  }

  for (uint16_t address: {uint16_t(0x0000), uint16_t(0x0010), uint16_t(0x0020)}) {
    QByteArray configBlock;
    if (!readConfigBlock(address, 0x0a, configBlock, err)) {
      errMsg(err) << "Cannot read DM1702S configuration block at " << Qt::hex << address << ".";
      return RadioInfo();
    }
  }

  if (!readValue(0x04, _firmware, err) || (_firmware.size() != 12)) {
    errMsg(err) << "Cannot read DM1702S firmware version.";
    return RadioInfo();
  }
  _radioInfo = RadioInfo::byID(RadioInfo::DM1702S);
  _identified = _radioInfo.isValid();
  return _radioInfo;
}

bool
DM1702SInterface::read_start(uint32_t bank, uint32_t addr, const ErrorStack &err) {
  Q_UNUSED(addr)
  if ((0 != bank) || !identifier(err).isValid() || _programMode)
    return false;

  QByteArray response;
  if (!send(QByteArray::fromHex("470000000001"), err)) {
    errMsg(err) << "Cannot send the DM1702S codeplug-read setup request.";
    return false;
  }
  if (!receiveAvailable(512, response, "setup", err)) {
    errMsg(err) << "Cannot receive the DM1702S codeplug-read setup response.";
    return false;
  }
  if (response.isEmpty() || (response[0] != 'S')) {
    errMsg(err) << "Invalid DM1702S codeplug-read setup response.";
    return false;
  }
  if (!acknowledge(err)) {
    errMsg(err) << "Cannot acknowledge the DM1702S setup response.";
    return false;
  }

  QByteArray memory;
  if (!readValue(0x0a, memory, err) || (memory.size() != 8)) {
    errMsg(err) << "Cannot read DM1702S memory bounds.";
    return false;
  }
  _memoryRange = qMakePair(littleEndian32(memory, 0), littleEndian32(memory, 4));
  if (_memoryRange.first > _memoryRange.second) {
    errMsg(err) << "Invalid DM1702S memory bounds.";
    return false;
  }

    if (!send(QByteArray::fromHex("ffffffff0c"), err) ||
      !send(QByteArray("DM1702S", 7), err) || !receiveExact(1, response, err) ||
      (static_cast<uint8_t>(response[0]) != 0x06) ||
      !send(QByteArray(1, '\x02'), err) || !receiveExact(8, response, err) ||
      (response != QByteArray(8, '\xff')) || !acknowledge(err)) {
    errMsg(err) << "Cannot enter DM1702S codeplug read mode.";
    return false;
  }
  _programMode = true;
  return true;
}

bool
DM1702SInterface::read(uint32_t bank, uint32_t addr, uint8_t *data, int nbytes,
                       const ErrorStack &err) {
  if ((0 != bank) || !_programMode || (nullptr == data) || (nbytes < 0))
    return false;

  while (nbytes > 0) {
    const int count = std::min(nbytes, maxReadPayload);
    QByteArray request(6, '\0');
    request[0] = 'R';
    request[1] = static_cast<char>(addr & 0xff);
    request[2] = static_cast<char>((addr >> 8) & 0xff);
    request[3] = static_cast<char>((addr >> 16) & 0xff);
    request[4] = static_cast<char>(count & 0xff);
    request[5] = static_cast<char>((count >> 8) & 0xff);

    QByteArray response;
    if (!send(request, err))
      return false;
    while (response.size() < 6) {
      QByteArray fragment;
      if (!receiveAvailable(512, fragment, "R response header", err)) {
        errMsg(err) << "Cannot receive the DM1702S response header at " << Qt::hex << addr << "h.";
        return false;
      }
      response.append(fragment);
    }
    const uint32_t responseAddress = static_cast<uint8_t>(response[1]) |
        (static_cast<uint32_t>(static_cast<uint8_t>(response[2])) << 8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(response[3])) << 16);
    const unsigned int responseLength = static_cast<uint8_t>(response[4]) |
        (static_cast<unsigned int>(static_cast<uint8_t>(response[5])) << 8);
    if ((response[0] != 'W') || (responseAddress != addr) || !responseLength ||
        (responseLength > static_cast<unsigned int>(count))) {
      errMsg(err) << "Unexpected DM1702S response for read at " << Qt::hex << addr << ".";
      return false;
    }

    const int frameSize = 6 + static_cast<int>(responseLength);
    const int receivedPayload = response.size()-6;
    if (receivedPayload > static_cast<int>(responseLength)) {
      errMsg(err) << "Oversized DM1702S response frame at " << Qt::hex << addr << "h.";
      return false;
    }
    QByteArray remainingPayload;
    const int remainingLength = static_cast<int>(responseLength)-receivedPayload;
    if ((remainingLength > 0) && (!receiveExact(remainingLength, remainingPayload, err))) {
      errMsg(err) << "Cannot receive the remaining " << remainingLength
                  << " bytes of the DM1702S R frame at " << Qt::hex << addr << "h.";
      return false;
    }
    response.append(remainingPayload);
    if (response.size() != frameSize) {
      errMsg(err) << "Unexpected DM1702S frame size at " << Qt::hex << addr << "h.";
      return false;
    }

    std::memcpy(data, response.constData()+6, responseLength);
    if (!acknowledge(err))
      return false;

    addr += responseLength;
    data += responseLength;
    nbytes -= static_cast<int>(responseLength);
  }
  return true;
}

bool
DM1702SInterface::read_finish(const ErrorStack &err) {
  Q_UNUSED(err)
  return _programMode;
}

bool
DM1702SInterface::write_start(uint32_t bank, uint32_t addr, const ErrorStack &err) {
  Q_UNUSED(addr)
  if ((0 != bank) || !_programMode) {
    errMsg(err) << "DM1702S is not in codeplug program mode.";
    return false;
  }
  return true;
}

bool
DM1702SInterface::write(uint32_t bank, uint32_t addr, uint8_t *data, int nbytes,
                        const ErrorStack &err) {
  if ((0 != bank) || !_programMode || (nullptr == data) || (nbytes < 0) || (addr > 0xffffff))
    return false;

  while (nbytes > 0) {
    const int count = std::min(nbytes, maxReadPayload);
    QByteArray request(6+count, '\0');
    request[0] = 'W';
    request[1] = static_cast<char>(addr & 0xff);
    request[2] = static_cast<char>((addr >> 8) & 0xff);
    request[3] = static_cast<char>((addr >> 16) & 0xff);
    request[4] = static_cast<char>(count & 0xff);
    request[5] = static_cast<char>((count >> 8) & 0xff);
    std::memcpy(request.data()+6, data, count);
    if (!send(request, err)) {
      errMsg(err) << "Cannot write DM1702S memory at " << Qt::hex << addr << "h.";
      return false;
    }
    QByteArray response;
    if (!receiveExact(1, response, err) || (static_cast<uint8_t>(response[0]) != 0x06)) {
      errMsg(err) << "Unexpected DM1702S write acknowledgement at " << Qt::hex << addr << "h.";
      return false;
    }
    addr += count;
    data += count;
    nbytes -= count;
  }
  return true;
}

bool
DM1702SInterface::write_finish(const ErrorStack &err) {
  Q_UNUSED(err)
  return _programMode;
}

QPair<uint32_t, uint32_t>
DM1702SInterface::memoryRange() const {
  return _memoryRange;
}

USBDeviceInfo
DM1702SInterface::interfaceInfo() {
  return USBDeviceInfo(USBDeviceInfo::Class::Bulk, vendorId, productId, false);
}

QList<USBDeviceDescriptor>
DM1702SInterface::detect(bool saveOnly) {
  QList<USBDeviceDescriptor> found;
  if (saveOnly)
    return found;

  libusb_context *context = nullptr;
  if (libusb_init(&context) < 0)
    return found;
  libusb_device **devices = nullptr;
  const ssize_t count = libusb_get_device_list(context, &devices);
  if (count >= 0) {
    for (ssize_t index=0; index<count; ++index) {
      libusb_device_descriptor descr{};
      if (libusb_get_device_descriptor(devices[index], &descr) < 0)
        continue;
      if ((descr.idVendor == vendorId) && (descr.idProduct == productId))
        found.append(Descriptor(interfaceInfo(), libusb_get_bus_number(devices[index]),
                                libusb_get_device_address(devices[index])));
    }
    libusb_free_device_list(devices, 1);
  }
  libusb_exit(context);
  return found;
}