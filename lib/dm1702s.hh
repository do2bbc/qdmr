#ifndef DM1702S_HH
#define DM1702S_HH

#include "dm1702s_codeplug.hh"
#include "radio.hh"

class DM1702SInterface;

class DM1702S : public Radio
{
  Q_OBJECT

public:
  explicit DM1702S(DM1702SInterface *device=nullptr, QObject *parent=nullptr);
  ~DM1702S() override;

  static RadioInfo defaultRadioInfo();
  const QString &name() const override;
  const RadioLimits &limits() const override;
  const Codeplug &codeplug() const override;
  Codeplug &codeplug() override;

  bool startDownload(const TransferFlags &flags, const ErrorStack &err=ErrorStack()) override;
  bool startDownloadCallsignDB(const TransferFlags &flags,
                               const ErrorStack &err=ErrorStack()) override;
  bool startUpload(Config *config, const Codeplug::Flags &flags,
                   const ErrorStack &err=ErrorStack()) override;
  bool startUploadCallsignDB(UserDatabase *db, const CallsignDB::Flags &selection,
                             const ErrorStack &err=ErrorStack()) override;
  bool startUploadSatelliteConfig(SatelliteDatabase *db, const TransferFlags &flags,
                                  const ErrorStack &err=ErrorStack()) override;

protected:
  void run() override;

private:
  bool download(const ErrorStack &err);
  bool downloadCallsignDatabase(const ErrorStack &err);
  bool upload(const ErrorStack &err);
  bool uploadCallsignDatabase(const ErrorStack &err);

  DM1702SInterface *_device;
  QString _name;
  DM1702SCodeplug _codeplug;
  Config *_config;
  Codeplug::Flags _codeplugFlags;
  QByteArray _callSignDatabase;
  QByteArray _downloadedCallsignDatabaseJson;
  static RadioLimits *_limits;
};

#endif // DM1702S_HH