#ifndef DM1702S_SETTINGSEXTENSION_HH
#define DM1702S_SETTINGSEXTENSION_HH

#include "configobject.hh"
#include "frequency.hh"

class DM1702SSettingsExtension : public ConfigItem
{
  Q_OBJECT

  Q_CLASSINFO("radioAliasDescription", "Text shown as the radio display alias.")
  Q_PROPERTY(QString radioAlias READ radioAlias WRITE setRadioAlias)
  Q_CLASSINFO("aprsEnabledDescription", "Enables APRS reporting.")
  Q_PROPERTY(bool aprsEnabled READ aprsEnabled WRITE setAprsEnabled)
  Q_CLASSINFO("dataTransmitServerDescription", "Data transmit server protocol.")
  Q_PROPERTY(DataTransmitServer dataTransmitServer READ dataTransmitServer WRITE setDataTransmitServer)
  Q_CLASSINFO("vfoARXFrequencyDescription", "VFO A receive frequency.")
  Q_PROPERTY(Frequency vfoARXFrequency READ vfoARXFrequency WRITE setVfoARXFrequency)
  Q_CLASSINFO("vfoATXFrequencyDescription", "VFO A transmit frequency.")
  Q_PROPERTY(Frequency vfoATXFrequency READ vfoATXFrequency WRITE setVfoATXFrequency)
  Q_CLASSINFO("vfoBRXFrequencyDescription", "VFO B receive frequency.")
  Q_PROPERTY(Frequency vfoBRXFrequency READ vfoBRXFrequency WRITE setVfoBRXFrequency)
  Q_CLASSINFO("vfoBTXFrequencyDescription", "VFO B transmit frequency.")
  Q_PROPERTY(Frequency vfoBTXFrequency READ vfoBTXFrequency WRITE setVfoBTXFrequency)

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
  Frequency vfoARXFrequency() const;
  void setVfoARXFrequency(const Frequency &frequency);
  Frequency vfoATXFrequency() const;
  void setVfoATXFrequency(const Frequency &frequency);
  Frequency vfoBRXFrequency() const;
  void setVfoBRXFrequency(const Frequency &frequency);
  Frequency vfoBTXFrequency() const;
  void setVfoBTXFrequency(const Frequency &frequency);

private:
  QString _radioAlias;
  bool _aprsEnabled;
  DataTransmitServer _dataTransmitServer;
  Frequency _vfoARXFrequency;
  Frequency _vfoATXFrequency;
  Frequency _vfoBRXFrequency;
  Frequency _vfoBTXFrequency;
};

#endif // DM1702S_SETTINGSEXTENSION_HH