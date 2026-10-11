#include "channel_extension.hh"



/* ******************************************************************************************* *
 * Implementation of common channel extended settings
 * ******************************************************************************************* */
ChannelExtension::ChannelExtension(QObject *parent)
  : ConfigExtension{parent}, _talkaround(false)
{
  // pass...
}


bool
ChannelExtension::talkaround() const {
  return _talkaround;
}
void
ChannelExtension::enableTalkaround(bool enable) {
  if (enable == _talkaround)
    return;
  _talkaround = enable;
  emit modified(this);
}



/* ******************************************************************************************* *
 * Implementation of common FM channel extended settings
 * ******************************************************************************************* */
FMChannelExtension::FMChannelExtension(QObject *parent)
  : ChannelExtension{parent}, _reverseBurst(false)
{
  // pass...
}


ConfigItem *
FMChannelExtension::clone() const {
  auto ext = new FMChannelExtension();
  if (! ext->copy(*this)) {
    ext->deleteLater();
    return nullptr;
  }
  return ext;
}


bool
FMChannelExtension::reverseBurst() const {
  return _reverseBurst;
}

void
FMChannelExtension::enableReverseBurst(bool enable) {
  if (enable == _reverseBurst)
    return;
  _reverseBurst = enable;
  emit modified(this);
}



/* ******************************************************************************************* *
 * Implementation of common DMR channel extended settings
 * ******************************************************************************************* */
DMRChannelExtension::DMRChannelExtension(QObject *parent)
  : ChannelExtension{parent}, _callConfirm(false), _sms(true), _smsConfirm(false),
  _dataConfirm(true), _dcdm(false), _loneWorker(false)
{
  // pass...
}


ConfigItem *
DMRChannelExtension::clone() const {
  auto ext = new DMRChannelExtension();
  if (! ext->copy(*this)) {
    ext->deleteLater();
    return nullptr;
  }
  return ext;
}


bool
DMRChannelExtension::privateCallConfirm() const {
  return _callConfirm;
}

void
DMRChannelExtension::enablePrivateCallConfirm(bool enabled) {
  if (enabled == _callConfirm)
    return;
  _callConfirm = enabled;
  emit modified(this);
}


bool
DMRChannelExtension::sms() const {
  return _sms;
}

void
DMRChannelExtension::enableSMS(bool enable) {
  if (enable == _sms)
    return;
  _sms = enable;
  emit modified(this);
}


bool
DMRChannelExtension::smsConfirm() const {
  return _smsConfirm;
}

void
DMRChannelExtension::enableSMSConfirm(bool enabled) {
  if (enabled == _smsConfirm)
    return;
  _smsConfirm = enabled;
  emit modified(this);
}


bool
DMRChannelExtension::dataConfirm() const {
  return _dataConfirm;
}

void
DMRChannelExtension::enableDataConfirm(bool enable) {
  if (enable==_dataConfirm)
    return;
  _dataConfirm = enable;
  emit modified(this);
}


bool
DMRChannelExtension::dcdm() const {
  return _dcdm;
}

void
DMRChannelExtension::enableDCDM(bool enable) {
  if (enable == _dcdm)
    return;
  _dcdm = enable;
  emit modified(this);
}


bool
DMRChannelExtension::loneWorker() const {
  return _loneWorker;
}

void
DMRChannelExtension::enableLoneWorker(bool enable) {
  if (enable == _loneWorker)
    return;
  _loneWorker = enable;
  emit modified(this);
}

DM1702SChannelExtension::DM1702SChannelExtension(QObject *parent)
  : ConfigExtension{parent}, _autoStartScan(false), _emergencyAlarmIndication(false),
    _emergencyAlarmAck(false), _emergencyCallIndication(false),
    _emergencySystem(EmergencySystem::None)
{
}

ConfigItem *
DM1702SChannelExtension::clone() const {
  auto *extension=new DM1702SChannelExtension();
  if (!extension->copy(*this)) {
    extension->deleteLater();
    return nullptr;
  }
  return extension;
}

bool
DM1702SChannelExtension::autoStartScan() const {
  return _autoStartScan;
}

void
DM1702SChannelExtension::enableAutoStartScan(bool enable) {
  if (enable == _autoStartScan)
    return;
  _autoStartScan = enable;
  emit modified(this);
}

bool
DM1702SChannelExtension::emergencyAlarmIndication() const {
  return _emergencyAlarmIndication;
}

void
DM1702SChannelExtension::enableEmergencyAlarmIndication(bool enable) {
  if (enable == _emergencyAlarmIndication)
    return;
  _emergencyAlarmIndication = enable;
  emit modified(this);
}

bool
DM1702SChannelExtension::emergencyAlarmAck() const {
  return _emergencyAlarmAck;
}

void
DM1702SChannelExtension::enableEmergencyAlarmAck(bool enable) {
  if (enable == _emergencyAlarmAck)
    return;
  _emergencyAlarmAck = enable;
  emit modified(this);
}

bool
DM1702SChannelExtension::emergencyCallIndication() const {
  return _emergencyCallIndication;
}

void
DM1702SChannelExtension::enableEmergencyCallIndication(bool enable) {
  if (enable == _emergencyCallIndication)
    return;
  _emergencyCallIndication = enable;
  emit modified(this);
}

DM1702SChannelExtension::EmergencySystem
DM1702SChannelExtension::emergencySystem() const {
  return _emergencySystem;
}

void
DM1702SChannelExtension::setEmergencySystem(EmergencySystem system) {
  if (static_cast<unsigned int>(system) > 15)
    system = EmergencySystem::None;
  if (system == _emergencySystem)
    return;
  _emergencySystem = system;
  emit modified(this);
}


