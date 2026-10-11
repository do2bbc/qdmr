#ifndef DM1702S_SETTINGSEXTENSION_HH
#define DM1702S_SETTINGSEXTENSION_HH

#include "configobject.hh"

class DM1702SSettingsExtension : public ConfigItem
{
  Q_OBJECT

  Q_CLASSINFO("radioAliasDescription", "Text shown as the radio display alias.")
  Q_PROPERTY(QString radioAlias READ radioAlias WRITE setRadioAlias)
  Q_CLASSINFO("aprsEnabledDescription", "Enables APRS reporting.")
  Q_PROPERTY(bool aprsEnabled READ aprsEnabled WRITE setAprsEnabled)
  Q_CLASSINFO("dataTransmitServerDescription", "Data transmit server protocol.")
  Q_PROPERTY(DataTransmitServer dataTransmitServer READ dataTransmitServer WRITE setDataTransmitServer)

public:
  enum class DataTransmitServer {
    HyteraDefinedData = 0, MOTOIP = 1
  };
  Q_ENUM(DataTransmitServer)

  explicit DM1702SSettingsExtension(QObject *parent=nullptr);

  ConfigItem *clone() const override;

  QString radioAlias() const;
  void setRadioAlias(const QString &alias);
  bool aprsEnabled() const;
  void setAprsEnabled(bool enable);
  DataTransmitServer dataTransmitServer() const;
  void setDataTransmitServer(DataTransmitServer server);

private:
  QString _radioAlias;
  bool _aprsEnabled;
  DataTransmitServer _dataTransmitServer;
};

#endif // DM1702S_SETTINGSEXTENSION_HH