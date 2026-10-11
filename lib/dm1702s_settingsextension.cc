#include "dm1702s_settingsextension.hh"

DM1702SSettingsExtension::DM1702SSettingsExtension(QObject *parent)
  : ConfigItem(parent), _radioAlias(), _aprsEnabled(false),
    _dataTransmitServer(DataTransmitServer::HyteraDefinedData)
{
}

ConfigItem *
DM1702SSettingsExtension::clone() const {
  auto *extension = new DM1702SSettingsExtension();
  if (!extension->copy(*this)) {
    delete extension;
    return nullptr;
  }
  return extension;
}

QString
DM1702SSettingsExtension::radioAlias() const {
  return _radioAlias;
}

void
DM1702SSettingsExtension::setRadioAlias(const QString &alias) {
  if (_radioAlias == alias)
    return;
  _radioAlias = alias;
  emit modified(this);
}

bool
DM1702SSettingsExtension::aprsEnabled() const {
  return _aprsEnabled;
}

void
DM1702SSettingsExtension::setAprsEnabled(bool enable) {
  if (_aprsEnabled == enable)
    return;
  _aprsEnabled = enable;
  emit modified(this);
}

DM1702SSettingsExtension::DataTransmitServer
DM1702SSettingsExtension::dataTransmitServer() const {
  return _dataTransmitServer;
}

void
DM1702SSettingsExtension::setDataTransmitServer(DataTransmitServer server) {
  if (_dataTransmitServer == server)
    return;
  _dataTransmitServer = server;
  emit modified(this);
}