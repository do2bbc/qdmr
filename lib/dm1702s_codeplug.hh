#ifndef DM1702S_CODEPLUG_HH
#define DM1702S_CODEPLUG_HH

#include "codeplug.hh"
#include "dm1702s_settingsextension.hh"

#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVector>

class DM1702SCodeplug : public Codeplug
{
  Q_OBJECT

public:
  explicit DM1702SCodeplug(QObject *parent=nullptr);

  void clearPages();
  bool setPage(uint8_t marker, const QByteArray &page, const ErrorStack &err=ErrorStack());
  bool setPageAtAddress(uint8_t marker, const QByteArray &page, uint32_t address,
                        const ErrorStack &err=ErrorStack());
  QByteArray page(uint8_t marker) const;
  uint32_t pageAddress(uint8_t marker) const;
  QList<uint8_t> pageMarkers() const;
  bool decode(Config *config, const ErrorStack &err=ErrorStack()) override;
  bool encode(Config *config, const Flags &flags=Flags(),
              const ErrorStack &err=ErrorStack()) override;
  bool index(Config *config, Context &ctx, const ErrorStack &err=ErrorStack()) const override;

private:
  struct ContactRecord {
    unsigned int number;
    uint8_t type;
    size_t dataOffset;
    QString name;
  };

  QMap<uint8_t, QByteArray> _pages;
  QMap<uint8_t, uint32_t> _pageAddresses;
  QStringList _originalChannelNames;
  QVector<int> _originalChannelContactIndexes;
  QVector<int> _originalChannelGPSIndexes;
  QVector<int> _originalChannelScanListIndexes;
  QVector<ContactRecord> _originalContacts;
  QVector<size_t> _originalRXGroupOffsets;
  QStringList _originalRXGroupNames;
  QVector<QVector<unsigned int>> _originalRXGroupMembers;
  QStringList _originalScanListNames;
  QVector<QVector<unsigned int>> _originalScanListMembers;
  QVector<int> _originalScanListPrimaryIndexes;
  QVector<int> _originalScanListSecondaryIndexes;
  QStringList _originalZoneNames;
  QVector<QVector<unsigned int>> _originalZoneAMembers;
  QVector<QVector<unsigned int>> _originalZoneBMembers;
  QStringList _originalGPSSystemNames;
  QString _originalRadioAlias;
  bool _originalAprsEnabled;
  DM1702SSettingsExtension::DataTransmitServer _originalDataTransmitServer;
  Frequency _originalVfoARXFrequency;
  Frequency _originalVfoATXFrequency;
  Frequency _originalVfoBRXFrequency;
  Frequency _originalVfoBTXFrequency;
  QString _originalConfigYAML;
};

#endif // DM1702S_CODEPLUG_HH