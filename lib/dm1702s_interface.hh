#ifndef DM1702S_INTERFACE_HH
#define DM1702S_INTERFACE_HH

#include "radiointerface.hh"

#include <QObject>
#include <QByteArray>
#include <QPair>
#include <libusb.h>

class DM1702SInterface : public QObject, public RadioInterface
{
  Q_OBJECT

public:
  class Descriptor: public USBDeviceDescriptor
  {
  public:
    Descriptor(const USBDeviceInfo &info, uint8_t bus, uint8_t device);
  };

  explicit DM1702SInterface(const USBDeviceDescriptor &descr,
                            const ErrorStack &err=ErrorStack(), QObject *parent=nullptr);
  ~DM1702SInterface() override;

  bool isOpen() const override;
  void close() override;
  RadioInfo identifier(const ErrorStack &err=ErrorStack()) override;
  bool write_start(uint32_t bank, uint32_t addr, const ErrorStack &err=ErrorStack()) override;
  bool write(uint32_t bank, uint32_t addr, uint8_t *data, int nbytes,
             const ErrorStack &err=ErrorStack()) override;
  bool write_finish(const ErrorStack &err=ErrorStack()) override;
  bool read_start(uint32_t bank, uint32_t addr, const ErrorStack &err=ErrorStack()) override;
  bool read(uint32_t bank, uint32_t addr, uint8_t *data, int nbytes,
            const ErrorStack &err=ErrorStack()) override;
  bool read_finish(const ErrorStack &err=ErrorStack()) override;

  QPair<uint32_t, uint32_t> memoryRange() const;

  static USBDeviceInfo interfaceInfo();
  static QList<USBDeviceDescriptor> detect(bool saveOnly=true);

private:
  bool send(const QByteArray &data, const ErrorStack &err);
  bool receiveExact(int length, QByteArray &data, const ErrorStack &err);
  bool receiveAvailable(int capacity, QByteArray &data, const char *operation,
                       const ErrorStack &err);
  bool acknowledge(const ErrorStack &err);
  bool readConfigBlock(uint16_t address, uint8_t length, QByteArray &data,
                       const ErrorStack &err);
  bool readValue(uint8_t id, QByteArray &value, const ErrorStack &err);

  libusb_context *_context;
  libusb_device_handle *_device;
  bool _detachedKernelDriver;
  bool _identified;
  bool _programMode;
  RadioInfo _radioInfo;
  QByteArray _firmware;
  QPair<uint32_t, uint32_t> _memoryRange;
};

#endif // DM1702S_INTERFACE_HH