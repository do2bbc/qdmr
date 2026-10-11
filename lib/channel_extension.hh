#ifndef CHANNEL_EXTENSION_HH
#define CHANNEL_EXTENSION_HH

#include "configobject.hh"


/** Common extended settings for all channels.
 * @ingroup conf */
class ChannelExtension : public ConfigExtension
{
  Q_OBJECT

  /** If @c true, talkaround is enabled. */
  Q_PROPERTY(bool talkaround READ talkaround WRITE enableTalkaround)

protected:
  /** Hidden constructor. */
  explicit ChannelExtension(QObject *parent = nullptr);


public:
  /** Returns @c true, if talkaround is enabled. */
  bool talkaround() const;
  /** Enables/disables talkaround. */
  void enableTalkaround(bool enable);

protected:
  /** If @c true, talkaround is enabled. */
  bool _talkaround;
};



/** Common FM channel extended settings.
 * @ingroup conf */
class FMChannelExtension: public ChannelExtension
{
  Q_OBJECT

  /** If @c true, the CTCSS phase-reverse burst at the end of transmission is enabled. */
  Q_PROPERTY(bool reverseBurst READ reverseBurst WRITE enableReverseBurst)

public:
  /** Default constructor. */
  explicit FMChannelExtension(QObject *parent = nullptr);

  ConfigItem *clone() const override;


  /** Returns @c true, if the CTCSS phase-reverse burst is enabled. */
  bool reverseBurst() const;
  /** Enables/disables the CTCSS phase-reverse burst. */
  void enableReverseBurst(bool enable);

protected:
  /** If @c true, the CTCSS phase-reverse burst at the end of transmission is enabled. */
  bool _reverseBurst;
};


/** Common DMR channel extended settings.
 * @ingroup conf */
class DMRChannelExtension: public ChannelExtension
{
  Q_OBJECT

  /** If @c true, the call confirmation is enabled. */
  Q_PROPERTY(bool privateCallConfirm READ privateCallConfirm WRITE enablePrivateCallConfirm)
  /** If @c true, SMS reception is enabled. */
  Q_PROPERTY(bool sms READ sms WRITE enableSMS)
  /** If @c true, the SMS confirmation is enabled. */
  Q_PROPERTY(bool smsConfirm READ smsConfirm WRITE enableSMSConfirm)
  /** If @c true, the radio will response to received data packages. Should be enabled. */
  Q_PROPERTY(bool dataConfirm READ dataConfirm WRITE enableDataConfirm)
  /** If @c true, the simplex DCDM mode is enabled. */
  Q_PROPERTY(bool dcdm READ dcdm WRITE enableDCDM)
  /** If @c true, the lone-worker feature is enabled for this channel. */
  Q_PROPERTY(bool loneWorker READ loneWorker WRITE enableLoneWorker)
public:
  /** Default constructor. */
  explicit DMRChannelExtension(QObject *parent=nullptr);

  ConfigItem *clone() const override;

  /** Returns @c true if the private-call confirmation is enabled. */
  bool privateCallConfirm() const;
  /** Enables/disables the private-call confirmation. */
  void enablePrivateCallConfirm(bool enabled);

  /** Returns @c true if SMS reception is enabled. */
  bool sms() const;
  /** Enables/disables SMS reception. */
  void enableSMS(bool enable);

  /** Returns @c true if the SMS confirmation is enabled. */
  bool smsConfirm() const;
  /** Enables/disables the SMS confirmation. */
  void enableSMSConfirm(bool enabled);

  /** Returns @c true if the data acknowledgement is enabled. */
  bool dataConfirm() const;
  /** Enables/disables the data acknowledgement. */
  void enableDataConfirm(bool enable);

  /** Returns @c true if the simplex DCDM mode is enabled. */
  bool dcdm() const;
  /** Enables/disables the simplex DCDM mode. */
  void enableDCDM(bool enable);

  /** Returns @c true if the lone-worker feature is enabled. */
  bool loneWorker() const;
  /** Enables the lone-worker feature for this channel. */
  void enableLoneWorker(bool enable);
protected:
  /** If @c true, the call confirmation is enabled. */
  bool _callConfirm;
  /** If @c true, the SMS reception is enabled. */
  bool _sms;
  /** If @c true, the SMS confirmation is enabled. */
  bool _smsConfirm;
  /** If @c true, the data acknowledgement is enabled. */
  bool _dataConfirm;
  /** If @c true, the simplex TDMA mode is enabled. */
  bool _dcdm;
  /** If @c true the lone-worker feature is enabled. */
  bool _loneWorker;
};


/** DM-1702S-specific DMR channel settings. */
class DM1702SChannelExtension: public ConfigExtension
{
  Q_OBJECT

  Q_CLASSINFO("autoStartScanDescription", "Starts scanning automatically when this channel is selected.")
  Q_PROPERTY(bool autoStartScan READ autoStartScan WRITE enableAutoStartScan)
  Q_CLASSINFO("emergencyAlarmIndicationDescription", "Enables emergency alarm indication.")
  Q_PROPERTY(bool emergencyAlarmIndication READ emergencyAlarmIndication WRITE enableEmergencyAlarmIndication)
  Q_CLASSINFO("emergencyAlarmAckDescription", "Enables emergency alarm acknowledgement.")
  Q_PROPERTY(bool emergencyAlarmAck READ emergencyAlarmAck WRITE enableEmergencyAlarmAck)
  Q_CLASSINFO("emergencyCallIndicationDescription", "Enables emergency call indication.")
  Q_PROPERTY(bool emergencyCallIndication READ emergencyCallIndication WRITE enableEmergencyCallIndication)
  Q_CLASSINFO("emergencySystemDescription", "Emergency system selection; None or system 1 through 15.")
  Q_PROPERTY(EmergencySystem emergencySystem READ emergencySystem WRITE setEmergencySystem)

public:
  enum class EmergencySystem {
    None = 0,
    System1 = 1, System2, System3, System4, System5,
    System6, System7, System8, System9, System10,
    System11, System12, System13, System14, System15
  };
  Q_ENUM(EmergencySystem)

  Q_INVOKABLE explicit DM1702SChannelExtension(QObject *parent=nullptr);
  ConfigItem *clone() const override;

  bool autoStartScan() const;
  void enableAutoStartScan(bool enable);
  bool emergencyAlarmIndication() const;
  void enableEmergencyAlarmIndication(bool enable);
  bool emergencyAlarmAck() const;
  void enableEmergencyAlarmAck(bool enable);
  bool emergencyCallIndication() const;
  void enableEmergencyCallIndication(bool enable);
  EmergencySystem emergencySystem() const;
  void setEmergencySystem(EmergencySystem system);

private:
  bool _autoStartScan;
  bool _emergencyAlarmIndication;
  bool _emergencyAlarmAck;
  bool _emergencyCallIndication;
  EmergencySystem _emergencySystem;
};


#endif // CHANNEL_EXTENSION_HH
