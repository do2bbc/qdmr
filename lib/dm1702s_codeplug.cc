#include "dm1702s_codeplug.hh"

#include "config.hh"
#include "dm32uv_codeplug.hh"
#include "logger.hh"

#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <QDate>
#include <QTextStream>

namespace {
constexpr size_t sectorSize = 0x1000;
constexpr size_t channelRecordSize = 0x30;
constexpr size_t channelNameSize = 11;

struct PageSlice {
  uint8_t marker;
  size_t begin;
};

bool appendPages(const QMap<uint8_t, QByteArray> &pages,
                 const std::vector<PageSlice> &slices, QByteArray &data,
                 const ErrorStack &err) {
  data.clear();
  for (const PageSlice &slice: slices) {
    auto page = pages.constFind(slice.marker);
    if ((page == pages.cend()) || (page->size() != static_cast<int>(sectorSize)) ||
        (page->at(sectorSize-1) != static_cast<char>(slice.marker))) {
      errMsg(err) << "Missing or invalid DM1702S page " << Qt::hex << slice.marker << ".";
      return false;
    }
    data.append(page->constData()+slice.begin,
                static_cast<int>(sectorSize-1-slice.begin));
  }
  return true;
}

QString readChannelName(const QByteArray &names, size_t index) {
  const size_t offset = index*channelNameSize;
  if (offset+channelNameSize > static_cast<size_t>(names.size()))
    return {};
  size_t length = 0;
  while ((length < channelNameSize) && (names[offset+length] != '\0') &&
         (static_cast<unsigned char>(names[offset+length]) != 0xff))
    ++length;
  return QString::fromLatin1(names.constData()+offset, static_cast<int>(length));
}

bool writePages(QMap<uint8_t, QByteArray> &pages, const std::vector<PageSlice> &slices,
                const QByteArray &data, const ErrorStack &err) {
  size_t dataOffset = 0;
  for (const PageSlice &slice: slices) {
    auto page = pages.find(slice.marker);
    if ((page == pages.end()) || (page->size() != static_cast<int>(sectorSize)) ||
        (page->at(sectorSize-1) != static_cast<char>(slice.marker))) {
      errMsg(err) << "Missing or invalid DM1702S page " << Qt::hex << slice.marker << ".";
      return false;
    }
    const size_t length = sectorSize-1-slice.begin;
    if ((dataOffset+length) > static_cast<size_t>(data.size())) {
      errMsg(err) << "DM1702S page data is truncated.";
      return false;
    }
    std::memcpy(page->data()+slice.begin, data.constData()+dataOffset, length);
    dataOffset += length;
  }
  return dataOffset == static_cast<size_t>(data.size());
}

bool appendRanges(const QMap<uint8_t, QByteArray> &pages,
                  const std::vector<std::pair<uint8_t, size_t>> &ranges,
                  QByteArray &data, const ErrorStack &err) {
  data.clear();
  for (const auto &range: ranges) {
    auto page = pages.constFind(range.first);
    if ((page == pages.cend()) || (page->size() != static_cast<int>(sectorSize)) ||
        (page->at(sectorSize-1) != static_cast<char>(range.first)) ||
        (range.second > sectorSize-1)) {
      errMsg(err) << "Missing or invalid DM1702S page " << Qt::hex << range.first << ".";
      return false;
    }
    data.append(page->constData(), static_cast<int>(range.second));
  }
  return true;
}

bool writeRanges(QMap<uint8_t, QByteArray> &pages,
                 const std::vector<std::pair<uint8_t, size_t>> &ranges,
                 const QByteArray &data, const ErrorStack &err) {
  size_t dataOffset = 0;
  for (const auto &range: ranges) {
    auto page = pages.find(range.first);
    if ((page == pages.end()) || (page->size() != static_cast<int>(sectorSize)) ||
        (page->at(sectorSize-1) != static_cast<char>(range.first)) ||
        (range.second > sectorSize-1) ||
        (dataOffset+range.second > static_cast<size_t>(data.size()))) {
      errMsg(err) << "Missing or invalid DM1702S page " << Qt::hex << range.first << ".";
      return false;
    }
    std::memcpy(page->data(), data.constData()+dataOffset, range.second);
    dataOffset += range.second;
  }
  return dataOffset == static_cast<size_t>(data.size());
}

QString readLatin1(const QByteArray &data, size_t offset, size_t maxLength) {
  size_t length = 0;
  while ((length < maxLength) && (data[offset+length] != '\0') &&
         (static_cast<unsigned char>(data[offset+length]) != 0xff))
    ++length;
  return QString::fromLatin1(data.constData()+offset, static_cast<int>(length));
}

bool readBCD24Frequency(const QByteArray &data, size_t offset, Frequency &frequency,
                        const ErrorStack &err) {
  if (offset+3 > static_cast<size_t>(data.size())) {
    errMsg(err) << "DM1702S VFO frequency is outside its page.";
    return false;
  }
  uint32_t value=0;
  uint32_t multiplier=1;
  for (size_t index=0; index<3; ++index) {
    const uint8_t byte=static_cast<uint8_t>(data[offset+index]);
    const uint8_t low=byte & 0x0f;
    const uint8_t high=(byte >> 4) & 0x0f;
    if ((low > 9) || (high > 9)) {
      errMsg(err) << "Invalid BCD digit in DM1702S VFO frequency.";
      return false;
    }
    value += static_cast<uint32_t>(low)*multiplier;
    multiplier *= 10;
    value += static_cast<uint32_t>(high)*multiplier;
    multiplier *= 10;
  }
  frequency=Frequency::fromHz(static_cast<qint64>(value)*1000);
  return true;
}

bool writeBCD24Frequency(QByteArray &data, size_t offset, const Frequency &frequency,
                         const ErrorStack &err) {
  const qint64 hertz=frequency.inHz();
  if ((hertz < 0) || (hertz % 1000) || (hertz/1000 > 999999) ||
      (offset+3 > static_cast<size_t>(data.size()))) {
    errMsg(err) << "DM1702S VFO frequencies must be in 1 kHz steps and fit six BCD digits.";
    return false;
  }
  uint32_t value=static_cast<uint32_t>(hertz/1000);
  for (size_t index=0; index<3; ++index) {
    const uint8_t low=value % 10;
    value /= 10;
    const uint8_t high=value % 10;
    value /= 10;
    data[offset+index]=static_cast<char>((high << 4) | low);
  }
  return true;
}

class DM1702SChannelElement final : public DM32UVCodeplug::ChannelElement {
public:
  explicit DM1702SChannelElement(uint8_t *data)
    : DM32UVCodeplug::ChannelElement(data) {}

  ChannelType channelType() const override {
    return getBit(0x18, 6) ? ChannelType::DMR : ChannelType::FM;
  }

  Channel::Power power() const override {
    return getBit(0x18, 1) ? Channel::Power::High : Channel::Power::Low;
  }

  bool loneWorkerEnabled() const override {
    return getBit(0x18, 2);
  }

  FMChannel::Bandwidth bandwidth() const override {
    return getBit(0x18, 0) ? FMChannel::Bandwidth::Wide : FMChannel::Bandwidth::Narrow;
  }

  bool validScanListIndex() const override {
    return 0 != getUInt6(0x19, 0);
  }

  unsigned int scanListIndex() const override {
    return getUInt6(0x19, 0)-1;
  }

  void setScanListIndex(unsigned int index) override {
    setUInt6(0x19, 0, static_cast<uint8_t>(index+1));
  }

  void clearScanListIndex() override {
    setUInt6(0x19, 0, 0);
  }

  DM32UVCodeplug::ChannelElement::Admit admitCriterion() const override {
    return static_cast<Admit>(getUInt2(0x1a, channelType() == ChannelType::DMR ? 3 : 0));
  }

  bool emergencyNotificationEnabled() const override {
    return getBit(0x1b, 7);
  }

  bool emergencyACKEnabled() const override {
    return getBit(0x1b, 6);
  }

  bool validEmergencySystemIndex() const override {
    return 0 != getUInt4(0x1b, 0);
  }

  unsigned int emergencySystemIndex() const override {
    return getUInt4(0x1b, 0)-1;
  }

  bool rxOnlyEnabled() const override {
    return getBit(0x18, 5);
  }

  bool privateCallACKEnabled() const override {
    return getBit(0x1d, 5);
  }

  bool dataACKEnabled() const override {
    return getBit(0x1d, 4);
  }

  bool dcdmEnabled() const override {
    return getBit(0x1e, 5);
  }

  DMRChannel::TimeSlot timeslot() const override {
    return getBit(0x1e, 4) ? DMRChannel::TimeSlot::TS2 : DMRChannel::TimeSlot::TS1;
  }

  unsigned int colorCode() const override {
    return getUInt4(0x1e, 0);
  }

  bool hasKeyIndex() const override {
    return 0 != getUInt4(0x1f, 0);
  }

  unsigned int keyIndex() const override {
    return getUInt4(0x1f, 0)-1;
  }

  bool encryptionEnabled() const override {
    return getBit(0x1f, 7);
  }

  SelectiveCall rxTone() const override {
    return decodeSelectiveCall(getUInt16_le(0x23));
  }

  SelectiveCall txTone() const override {
    return decodeSelectiveCall(getUInt16_le(0x25));
  }

  bool voxEnabled() const override {
    return getBit(0x27, 4);
  }

  bool talkaroundEnabled() const override {
    return getBit(0x2b, 6);
  }
};
}

DM1702SCodeplug::DM1702SCodeplug(QObject *parent)
  : Codeplug(parent), _pages(), _pageAddresses(), _originalChannelNames(), _originalRadioAlias(),
    _originalAprsEnabled(false),
    _originalDataTransmitServer(DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData),
    _originalVfoARXFrequency(), _originalVfoATXFrequency(), _originalVfoBRXFrequency(),
    _originalVfoBTXFrequency(),
    _originalConfigYAML()
{
  addImage(QStringLiteral("DM1702S"));
}

void
DM1702SCodeplug::clearPages() {
  _pages.clear();
  _pageAddresses.clear();
  _originalChannelNames.clear();
  _originalChannelContactIndexes.clear();
  _originalChannelGPSIndexes.clear();
  _originalChannelScanListIndexes.clear();
  _originalContacts.clear();
  _originalRXGroupOffsets.clear();
  _originalRXGroupNames.clear();
  _originalRXGroupMembers.clear();
  _originalScanListNames.clear();
  _originalScanListMembers.clear();
  _originalScanListPrimaryIndexes.clear();
  _originalScanListSecondaryIndexes.clear();
  _originalZoneNames.clear();
  _originalZoneAMembers.clear();
  _originalZoneBMembers.clear();
  _originalGPSSystemNames.clear();
  _originalRadioAlias.clear();
  _originalAprsEnabled = false;
  _originalDataTransmitServer = DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData;
  _originalVfoARXFrequency = Frequency();
  _originalVfoATXFrequency = Frequency();
  _originalVfoBRXFrequency = Frequency();
  _originalVfoBTXFrequency = Frequency();
  _originalConfigYAML.clear();
}

bool
DM1702SCodeplug::setPage(uint8_t marker, const QByteArray &page, const ErrorStack &err) {
  return setPageAtAddress(marker, page, std::numeric_limits<uint32_t>::max(), err);
}

bool
DM1702SCodeplug::setPageAtAddress(uint8_t marker, const QByteArray &page, uint32_t address,
                                 const ErrorStack &err) {
  if ((page.size() != static_cast<int>(sectorSize)) ||
      (page.at(sectorSize-1) != static_cast<char>(marker))) {
    errMsg(err) << "Invalid DM1702S page for marker " << Qt::hex << marker << ".";
    return false;
  }
  if (_pages.contains(marker)) {
    errMsg(err) << "Duplicate DM1702S page marker " << Qt::hex << marker << ".";
    return false;
  }
  _pages.insert(marker, page);
  _pageAddresses.insert(marker, address);
  return true;
}

QByteArray
DM1702SCodeplug::page(uint8_t marker) const {
  return _pages.value(marker);
}

uint32_t
DM1702SCodeplug::pageAddress(uint8_t marker) const {
  return _pageAddresses.value(marker, std::numeric_limits<uint32_t>::max());
}

QList<uint8_t>
DM1702SCodeplug::pageMarkers() const {
  return _pages.keys();
}

bool
DM1702SCodeplug::decode(Config *config, const ErrorStack &err) {
  if (nullptr == config) {
    errMsg(err) << "Cannot decode DM1702S codeplug without a configuration.";
    return false;
  }

  std::vector<PageSlice> channelSlices;
  for (uint8_t marker=0x16; marker<=0x22; ++marker)
    channelSlices.push_back({marker, 0});

  QByteArray channelData, channelNames;
  if (! appendPages(_pages, channelSlices, channelData, err) ||
      ! appendPages(_pages, {{0x24, 0}, {0x25, 3}, {0x26, 0}}, channelNames, err))
    return false;

  if (channelData.size() < 2) {
    errMsg(err) << "DM1702S channel data is truncated.";
    return false;
  }
  const size_t channelCount = static_cast<uint8_t>(channelData[0]) |
      (static_cast<size_t>(static_cast<uint8_t>(channelData[1])) << 8);
  if ((!channelCount) || (channelCount > static_cast<size_t>(channelData.size())/channelRecordSize) ||
      (channelCount > static_cast<size_t>(channelNames.size())/channelNameSize)) {
    errMsg(err) << "Invalid DM1702S channel count or incomplete channel banks.";
    return false;
  }

  QByteArray contactMap, contactData, channelContactData, scanListData, zoneData, rxListData;
  std::vector<std::pair<uint8_t, size_t>> contactSlices;
  for (uint8_t marker=0x28; marker<=0x2c; ++marker)
    contactSlices.emplace_back(marker, 0x0ff0);
  std::vector<std::pair<uint8_t, size_t>> zoneRanges{{0x45, 0x0f0c}};
  for (uint8_t marker=0x46; marker<=0x56; ++marker)
    zoneRanges.emplace_back(marker, 0x0efc);
  if (!appendRanges(_pages, {{0x0b, 0x0fff}}, contactMap, err) ||
      !appendRanges(_pages, contactSlices, contactData, err) ||
      !appendRanges(_pages, {{0x27, 0x0800}}, channelContactData, err) ||
      !appendRanges(_pages, {{0x13, 0x0720}}, scanListData, err) ||
      !appendRanges(_pages, zoneRanges, zoneData, err) ||
      !appendRanges(_pages, {{0x11, 0x0606}, {0x12, 0x073d}}, rxListData, err))
    return false;
  if (channelCount > static_cast<size_t>(channelContactData.size())/2) {
    errMsg(err) << "DM1702S channel TX-contact table is too short.";
    return false;
  }
  if ((contactMap.size() < 0x74) || (contactData.size() < 800*0x18)) {
    errMsg(err) << "DM1702S contact data is truncated.";
    return false;
  }
  const unsigned int zoneCount=static_cast<uint8_t>(zoneData[0]);
  constexpr size_t zoneRecordSize=0x112;
  if ((zoneCount > 250) ||
      (0x10+static_cast<size_t>(zoneCount)*zoneRecordSize > static_cast<size_t>(zoneData.size()))) {
    errMsg(err) << "Invalid DM1702S zone count or truncated zone data.";
    return false;
  }

  config->contacts()->clear();
  config->rxGroupLists()->clear();
  config->scanlists()->clear();
  config->zones()->clear();
  _originalContacts.clear();
  _originalScanListNames.clear();
  _originalScanListMembers.clear();
  _originalScanListPrimaryIndexes.clear();
  _originalScanListSecondaryIndexes.clear();
  QMap<unsigned int, DMRContact *> groupContacts;
  QMap<int, QVector<unsigned int>> scanListMembers;
  Context context(config);
  constexpr size_t scanListRecordSize = 0x39;
  constexpr size_t maxScanListCount = 32;
  const size_t scanListCount=static_cast<uint8_t>(scanListData[0]);
  if (scanListCount > maxScanListCount) {
    errMsg(err) << "Invalid DM1702S scan list count.";
    return false;
  }
  for (size_t index=0; index<scanListCount; ++index) {
    const size_t offset=index*scanListRecordSize;
    const QString name=readLatin1(scanListData, offset+1, 16);
    if (name.isEmpty() || std::any_of(name.cbegin(), name.cend(), [](QChar character) {
          return (character.unicode() < 0x20) || (character.unicode() > 0x7e);
        })) {
      errMsg(err) << "Invalid DM1702S scan list name at index " << (index+1) << ".";
      return false;
    }

    auto *scanList=new ScanList(name);
    config->scanlists()->add(scanList, -1, false);
    context.add(scanList, static_cast<unsigned int>(index));
    QVector<unsigned int> members;
    for (unsigned int member=0; member<15; ++member) {
      const size_t memberOffset=offset+0x1b+member*2;
      const unsigned int channelNumber=static_cast<uint8_t>(scanListData[memberOffset]) |
          (static_cast<unsigned int>(static_cast<uint8_t>(scanListData[memberOffset+1])) << 8);
      if ((0 == channelNumber) || (0xffff == channelNumber))
        break;
      if (channelNumber > channelCount) {
        errMsg(err) << "DM1702S scan list " << (index+1) << " references invalid channel "
                    << channelNumber << ".";
        return false;
      }
      members.append(channelNumber-1);
    }
    scanListMembers.insert(static_cast<int>(index), members);
    _originalScanListNames.append(name);
    _originalScanListMembers.append(members);
    const unsigned int primaryChannel=static_cast<uint8_t>(scanListData[offset+0x12]);
    const unsigned int secondaryChannel=static_cast<uint8_t>(scanListData[offset+0x13]);
    if ((primaryChannel > channelCount) || (secondaryChannel > channelCount)) {
      errMsg(err) << "DM1702S scan list " << (index+1) << " has an invalid priority-channel index.";
      return false;
    }
    _originalScanListPrimaryIndexes.append(primaryChannel ? static_cast<int>(primaryChannel)-1 : -1);
    _originalScanListSecondaryIndexes.append(secondaryChannel ? static_cast<int>(secondaryChannel)-1 : -1);
  }

  const QByteArray bitmap = contactMap.mid(0x10, 0x64);
  for (unsigned int slot=0; slot<800; ++slot) {
    if (static_cast<uint8_t>(bitmap[slot/8]) & (1u << (slot%8)))
      continue;
    const size_t offset = static_cast<size_t>(slot)*0x18;
    const uint32_t packed = static_cast<uint8_t>(contactData[offset+19]) |
        (static_cast<uint32_t>(static_cast<uint8_t>(contactData[offset+20])) << 8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(contactData[offset+21])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(contactData[offset+22])) << 24);
    const unsigned int number = packed & 0x00ffffff;
    const uint8_t type = static_cast<uint8_t>((packed >> 24) & 0x0f);
    if (!number)
      continue;
    DMRContact::Type contactType;
    switch (type) {
    case 0x03: contactType = DMRContact::PrivateCall; break;
    case 0x04: contactType = DMRContact::GroupCall; break;
    case 0x05: contactType = DMRContact::AllCall; break;
    default:
      errMsg(err) << "Unsupported DM1702S contact type " << Qt::hex << type << ".";
      return false;
    }
    const QString name = readLatin1(contactData, offset+2, 16);
    auto *contact = new DMRContact(contactType, name, number);
    config->contacts()->add(contact);
    if (DMRContact::GroupCall == contactType)
      groupContacts.insert(number, contact);
    _originalContacts.append({number, type, offset, name});
  }

  QVector<size_t> rxGroupOffsets;
  constexpr size_t groupRecordSize = 0x6d;
  for (size_t offset=0x0f; offset+groupRecordSize<=static_cast<size_t>(rxListData.size());
       offset+=groupRecordSize) {
    const QString name = readLatin1(rxListData, offset+2, 11);
    QVector<unsigned int> memberIds;
    for (unsigned int member=0; member<32; ++member) {
      const size_t memberOffset = offset+13+member*3;
      const unsigned int id = static_cast<uint8_t>(rxListData[memberOffset]) |
          (static_cast<unsigned int>(static_cast<uint8_t>(rxListData[memberOffset+1])) << 8) |
          (static_cast<unsigned int>(static_cast<uint8_t>(rxListData[memberOffset+2])) << 16);
      if (id)
        memberIds.append(id);
    }
    if (memberIds.isEmpty())
      continue;
    if (name.isEmpty() || std::any_of(name.cbegin(), name.cend(), [](QChar character) {
          return !character.isPrint();
        })) {
      errMsg(err) << "Unsupported non-printable DM1702S RX-group name.";
      return false;
    }
    auto *group = new RXGroupList(name);
    for (unsigned int id: memberIds) {
      DMRContact *contact = groupContacts.value(id, nullptr);
      if (nullptr == contact) {
        contact = new DMRContact(DMRContact::GroupCall, QStringLiteral("Group Call"), id);
        config->contacts()->add(contact);
        groupContacts.insert(id, contact);
        _originalContacts.append({id, 0x04, std::numeric_limits<size_t>::max(), contact->name()});
      }
      group->addContact(contact);
    }
    config->rxGroupLists()->add(group);
    rxGroupOffsets.append(offset);
  }
  if (rxGroupOffsets.isEmpty()) {
    errMsg(err) << "No DM1702S RX group lists were found.";
    return false;
  }

  const QByteArray settingsPage = _pages.value(0x04);
  if ((settingsPage.size() != static_cast<int>(sectorSize)) ||
      (settingsPage.at(sectorSize-1) != static_cast<char>(0x04))) {
    errMsg(err) << "Missing or invalid DM1702S settings page.";
    return false;
  }
  const unsigned int gpsSystemCount=static_cast<uint8_t>(settingsPage[0x600]);
  if (gpsSystemCount > 16) {
    errMsg(err) << "Invalid DM1702S GPS system count.";
    return false;
  }
  config->posSystems()->clear();
  _originalGPSSystemNames.clear();
  for (unsigned int index=0; index<gpsSystemCount; ++index) {
    const QString name=readLatin1(settingsPage, 0x500+index*0x10, 9);
    if (name.isEmpty() || std::any_of(name.cbegin(), name.cend(), [](QChar character) {
          return !character.isPrint();
        })) {
      errMsg(err) << "Invalid DM1702S GPS system name at index " << (index+1) << ".";
      return false;
    }
    config->posSystems()->add(new DMRAPRSSystem(name), -1, false);
    _originalGPSSystemNames.append(name);
  }
  const QByteArray vfoPage=_pages.value(0x78);
  if ((vfoPage.size() != static_cast<int>(sectorSize)) ||
      (vfoPage.at(sectorSize-1) != static_cast<char>(0x78))) {
    errMsg(err) << "Missing or invalid DM1702S VFO page.";
    return false;
  }
  if (!readBCD24Frequency(vfoPage, 0x0fa0, _originalVfoARXFrequency, err) ||
      !readBCD24Frequency(vfoPage, 0x0fa4, _originalVfoATXFrequency, err) ||
      !readBCD24Frequency(vfoPage, 0x0fd0, _originalVfoBRXFrequency, err) ||
      !readBCD24Frequency(vfoPage, 0x0fd4, _originalVfoBTXFrequency, err))
    return false;
  size_t aliasLength = 0;
  while ((aliasLength < 16) && (settingsPage[0x1d0+aliasLength] != '\0') &&
         (static_cast<unsigned char>(settingsPage[0x1d0+aliasLength]) != 0xff))
    ++aliasLength;
  _originalRadioAlias = QString::fromLatin1(settingsPage.constData()+0x1d0,
                                             static_cast<int>(aliasLength));
  _originalAprsEnabled = 0 != (static_cast<uint8_t>(settingsPage[0x44]) & 0x01);
  _originalDataTransmitServer = (0 != (static_cast<uint8_t>(settingsPage[0x700]) & 0x01))
      ? DM1702SSettingsExtension::DataTransmitServer::MOTOIP
      : DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData;
  if (nullptr == config->settings()->dm1702sExtension())
    config->settings()->setDM1702SExtension(new DM1702SSettingsExtension());
  config->settings()->dm1702sExtension()->setRadioAlias(_originalRadioAlias);
  config->settings()->dm1702sExtension()->setAprsEnabled(_originalAprsEnabled);
  config->settings()->dm1702sExtension()->setDataTransmitServer(_originalDataTransmitServer);
  config->settings()->dm1702sExtension()->setVfoARXFrequency(_originalVfoARXFrequency);
  config->settings()->dm1702sExtension()->setVfoATXFrequency(_originalVfoATXFrequency);
  config->settings()->dm1702sExtension()->setVfoBRXFrequency(_originalVfoBRXFrequency);
  config->settings()->dm1702sExtension()->setVfoBTXFrequency(_originalVfoBTXFrequency);
  _originalRXGroupOffsets = rxGroupOffsets;
  _originalRXGroupNames.clear();
  _originalRXGroupMembers.clear();
  for (int index=0; index<config->rxGroupLists()->count(); ++index) {
    RXGroupList *group = config->rxGroupLists()->list(index);
    _originalRXGroupNames.append(group->name());
    QVector<unsigned int> ids;
    for (int member=0; member<group->count(); ++member)
      ids.append(group->contact(member)->number());
    _originalRXGroupMembers.append(ids);
  }

  _originalChannelNames.clear();
  _originalChannelContactIndexes.clear();
  _originalChannelGPSIndexes.clear();
  _originalChannelScanListIndexes.clear();
  for (size_t index=0; index<channelCount; ++index) {
    const QString name = readChannelName(channelNames, index);
    if (name.isEmpty()) {
      errMsg(err) << "DM1702S channel " << (index+1) << " has no name.";
      return false;
    }

    std::array<uint8_t, channelRecordSize> record{};
    std::copy_n(reinterpret_cast<const uint8_t *>(channelData.constData())+
                  index*channelRecordSize, channelRecordSize, record.begin());
    DM1702SChannelElement element(record.data());
    element.setName(name);
    Channel *channel = element.decode(context, err);
    if (nullptr == channel) {
      errMsg(err) << "Cannot decode DM1702S channel " << (index+1) << ".";
      return false;
    }
    config->channelList()->add(channel);
    int scanListIndex=-1;
    if (element.validScanListIndex()) {
      scanListIndex=static_cast<int>(element.scanListIndex());
      if (!context.has<ScanList>(static_cast<unsigned int>(scanListIndex))) {
        errMsg(err) << "DM1702S channel " << (index+1) << " references invalid scan list "
                    << (scanListIndex+1) << ".";
        delete channel;
        return false;
      }
      channel->setScanList(context.get<ScanList>(static_cast<unsigned int>(scanListIndex)));
    }
    _originalChannelScanListIndexes.append(scanListIndex);
    if (channel->is<DMRChannel>()) {
      const unsigned int gpsPosition=element.dmrAPRSChannelIndex();
      int gpsIndex=-1;
      if (gpsPosition) {
        if (gpsPosition > static_cast<unsigned int>(config->posSystems()->count())) {
          errMsg(err) << "DM1702S channel " << (index+1) << " has invalid GPS system index "
                      << gpsPosition << ".";
          delete channel;
          return false;
        }
        gpsIndex=static_cast<int>(gpsPosition)-1;
        channel->as<DMRChannel>()->setAPRS(config->posSystems()->system(gpsIndex));
      }
      _originalChannelGPSIndexes.append(gpsIndex);
      const size_t contactOffset=index*2;
      const unsigned int contactPosition=
          ((static_cast<unsigned int>(static_cast<uint8_t>(channelContactData[contactOffset])) >> 4) << 8) |
          static_cast<uint8_t>(channelContactData[contactOffset+1]);
      int contactIndex=-1;
      if ((contactPosition != 0) && (contactPosition != 0x0fff)) {
        if (contactPosition > static_cast<unsigned int>(config->contacts()->count())) {
          errMsg(err) << "DM1702S channel " << (index+1) << " has invalid TX-contact index "
                      << contactPosition << ".";
          delete channel;
          return false;
        }
        contactIndex=static_cast<int>(contactPosition)-1;
        auto *contact=config->contacts()->contact(contactIndex)->as<DMRContact>();
        if ((nullptr == contact) || !channel->as<DMRChannel>()->setContact(contact)) {
          errMsg(err) << "Cannot assign DM1702S TX contact to channel " << (index+1) << ".";
          delete channel;
          return false;
        }
      }
      _originalChannelContactIndexes.append(contactIndex);
      const unsigned int groupIndex = record[0x21] & 0x3f;
      if (groupIndex) {
        if (groupIndex > static_cast<unsigned int>(config->rxGroupLists()->count())) {
          errMsg(err) << "DM1702S channel " << (index+1) << " has invalid RX-group index "
                      << groupIndex << ".";
          delete channel;
          return false;
        }
        channel->as<DMRChannel>()->setGroupList(config->rxGroupLists()->list(groupIndex-1));
      }
    } else {
      _originalChannelContactIndexes.append(-1);
      _originalChannelGPSIndexes.append(-1);
    }
    context.add(channel, static_cast<unsigned int>(index));
    _originalChannelNames.append(name);
  }

  for (auto list=scanListMembers.cbegin(); list!=scanListMembers.cend(); ++list) {
    ScanList *scanList=context.get<ScanList>(static_cast<unsigned int>(list.key()));
    for (unsigned int channelIndex: list.value())
      scanList->addChannel(config->channelList()->channel(static_cast<int>(channelIndex)));
    const int primaryIndex=_originalScanListPrimaryIndexes[list.key()];
    const int secondaryIndex=_originalScanListSecondaryIndexes[list.key()];
    if (primaryIndex >= 0)
      scanList->setPrimaryChannel(config->channelList()->channel(primaryIndex));
    if (secondaryIndex >= 0)
      scanList->setSecondaryChannel(config->channelList()->channel(secondaryIndex));
  }

  _originalZoneNames.clear();
  _originalZoneAMembers.clear();
  _originalZoneBMembers.clear();
  for (unsigned int zoneIndex=0; zoneIndex<zoneCount; ++zoneIndex) {
    const size_t offset=0x10+static_cast<size_t>(zoneIndex)*zoneRecordSize;
    const QString name=readLatin1(zoneData, offset, 16);
    const unsigned int aCount=static_cast<uint8_t>(zoneData[offset+0x10]);
    const unsigned int bCount=static_cast<uint8_t>(zoneData[offset+0x91]);
    if (name.isEmpty() || std::any_of(name.cbegin(), name.cend(), [](QChar character) {
          return !character.isPrint();
        }) || (aCount > 64) || (bCount > 64)) {
      errMsg(err) << "Invalid DM1702S zone record " << (zoneIndex+1) << ".";
      return false;
    }
    auto *zone=new Zone(name);
    config->zones()->add(zone, -1, false);
    QVector<unsigned int> aMembers, bMembers;
    auto readMembers=[&](size_t memberOffset, unsigned int count,
                         QVector<unsigned int> &members, ChannelRefList *list) {
      for (unsigned int member=0; member<count; ++member) {
        const unsigned int channelNumber=static_cast<uint8_t>(zoneData[memberOffset+2*member]) |
            (static_cast<unsigned int>(static_cast<uint8_t>(zoneData[memberOffset+2*member+1])) << 8);
        if ((channelNumber == 0) || (channelNumber > channelCount))
          return false;
        const unsigned int channelIndex=channelNumber-1;
        members.append(channelIndex);
        list->add(config->channelList()->channel(static_cast<int>(channelIndex)));
      }
      return true;
    };
    if (!readMembers(offset+0x11, aCount, aMembers, zone->A()) ||
        !readMembers(offset+0x92, bCount, bMembers, zone->B())) {
      errMsg(err) << "DM1702S zone " << (zoneIndex+1) << " references an invalid channel.";
      return false;
    }
    _originalZoneNames.append(name);
    _originalZoneAMembers.append(aMembers);
    _originalZoneBMembers.append(bMembers);
  }

  QString yaml;
  QTextStream stream(&yaml);
  if (!config->toYAML(stream, err))
    return false;
  _originalConfigYAML = yaml;
  return true;
}

bool
DM1702SCodeplug::encode(Config *config, const Flags &flags, const ErrorStack &err) {
  Q_UNUSED(flags)
  if ((nullptr == config) || _originalConfigYAML.isEmpty() || _originalChannelNames.isEmpty()) {
    errMsg(err) << "Cannot encode DM1702S without a decoded source codeplug.";
    return false;
  }
  if (config->channelList()->count() != _originalChannelNames.size()) {
    errMsg(err) << "Changing the number of DM1702S channels is not supported yet.";
    return false;
  }
  if (_originalChannelContactIndexes.size() != config->channelList()->count()) {
    errMsg(err) << "DM1702S source has incomplete channel TX-contact mappings.";
    return false;
  }
  if ((_originalChannelScanListIndexes.size() != config->channelList()->count()) ||
      (config->scanlists()->count() > 32) ||
      (_originalScanListNames.size() != _originalScanListMembers.size()) ||
      (_originalScanListNames.size() != _originalScanListPrimaryIndexes.size()) ||
      (_originalScanListNames.size() != _originalScanListSecondaryIndexes.size())) {
    errMsg(err) << "DM1702S scan list count or channel references are invalid.";
    return false;
  }
  for (int index=0; index<config->scanlists()->count(); ++index) {
    ScanList *scanList=config->scanlists()->scanlist(index);
    const QByteArray name=scanList->name().toLatin1();
    if (scanList->name().isEmpty() || (QString::fromLatin1(name) != scanList->name()) ||
        (name.size() > 16) || name.contains('\0') || (scanList->count() > 15) ||
        scanList->revertChannel() ||
        std::any_of(scanList->name().cbegin(), scanList->name().cend(), [](QChar character) {
          return (character.unicode() < 0x20) || (character.unicode() > 0x7e);
        })) {
      errMsg(err) << "DM1702S scan lists require printable names, at most 15 channels, and no revert channel.";
      return false;
    }
    for (int member=0; member<scanList->count(); ++member)
      if (config->channelList()->indexOf(scanList->channel(member)) < 0) {
        errMsg(err) << "DM1702S scan list references a channel outside the channel list.";
        return false;
      }
    auto validPriority=[&](Channel *channel) {
      return (nullptr == channel) || (config->channelList()->indexOf(channel) >= 0);
    };
    if (!validPriority(scanList->primaryChannel()) || !validPriority(scanList->secondaryChannel())) {
      errMsg(err) << "DM1702S priority channels must belong to the codeplug channel list.";
      return false;
    }
  }
  if ((_originalChannelGPSIndexes.size() != config->channelList()->count()) ||
      (config->posSystems()->count() < _originalGPSSystemNames.size()) ||
      (config->posSystems()->count() > 16)) {
    errMsg(err) << "DM1702S GPS system count or channel references are invalid.";
    return false;
  }
  for (int index=0; index<config->posSystems()->count(); ++index) {
    auto *system=config->posSystems()->system(index)->as<DMRAPRSSystem>();
    if (nullptr == system) {
      errMsg(err) << "DM1702S supports only DMR GPS systems.";
      return false;
    }
    const QByteArray name=system->name().toLatin1();
    if (system->name().isEmpty() || (QString::fromLatin1(name) != system->name()) ||
        (name.size() > 9) || name.contains('\0') || name.contains(static_cast<char>(0xff))) {
      errMsg(err) << "DM1702S GPS system names must contain 1 to 9 Latin-1 bytes.";
      return false;
    }
  }
  if (config->contacts()->count() != _originalContacts.size()) {
    errMsg(err) << "Adding or removing DM1702S contacts is not supported yet.";
    return false;
  }
  if ((config->rxGroupLists()->count() != _originalRXGroupOffsets.size()) ||
      (_originalRXGroupOffsets.size() != _originalRXGroupNames.size()) ||
      (_originalRXGroupOffsets.size() != _originalRXGroupMembers.size())) {
    errMsg(err) << "Adding or removing DM1702S RX group lists is not supported yet.";
    return false;
  }
  if ((config->zones()->count() < _originalZoneNames.size()) ||
      (config->zones()->count() > 250) ||
      (_originalZoneNames.size() != _originalZoneAMembers.size()) ||
      (_originalZoneNames.size() != _originalZoneBMembers.size())) {
    errMsg(err) << "Removing zones or exceeding the DM1702S zone limit is not supported.";
    return false;
  }
  for (int index=0; index<config->rxGroupLists()->count(); ++index) {
    RXGroupList *group = config->rxGroupLists()->list(index);
    if ((group->name() != _originalRXGroupNames[index]) || (group->count() > 32)) {
      errMsg(err) << "Changing DM1702S RX group names or exceeding 32 members is not supported.";
      return false;
    }
    for (int member=0; member<group->count(); ++member) {
      DMRContact *contact = group->contact(member);
      if ((nullptr == contact) || (DMRContact::GroupCall != contact->type()) ||
          (contact->number() > 0xffffff)) {
        errMsg(err) << "DM1702S RX group members must be 24-bit group-call contacts.";
        return false;
      }
    }
  }

  Config normalized;
  if (!normalized.copy(*config)) {
    errMsg(err) << "Cannot copy DM1702S configuration for validation.";
    return false;
  }
  if (nullptr == normalized.settings()->dm1702sExtension()) {
    errMsg(err) << "Missing DM1702S radio settings.";
    return false;
  }
  normalized.settings()->dm1702sExtension()->setRadioAlias(_originalRadioAlias);
  normalized.settings()->dm1702sExtension()->setAprsEnabled(_originalAprsEnabled);
  normalized.settings()->dm1702sExtension()->setDataTransmitServer(_originalDataTransmitServer);
  normalized.settings()->dm1702sExtension()->setVfoARXFrequency(_originalVfoARXFrequency);
  normalized.settings()->dm1702sExtension()->setVfoATXFrequency(_originalVfoATXFrequency);
  normalized.settings()->dm1702sExtension()->setVfoBRXFrequency(_originalVfoBRXFrequency);
  normalized.settings()->dm1702sExtension()->setVfoBTXFrequency(_originalVfoBTXFrequency);
  for (int index=0; index<_originalGPSSystemNames.size(); ++index) {
    auto *system=normalized.posSystems()->system(index)->as<DMRAPRSSystem>();
    if (nullptr == system) {
      errMsg(err) << "Cannot normalize DM1702S GPS system " << (index+1) << ".";
      return false;
    }
    system->setName(_originalGPSSystemNames[index]);
  }

  for (int index=0; index<config->contacts()->count(); ++index) {
    auto *contact = config->contacts()->contact(index)->as<DMRContact>();
    auto *copy = normalized.contacts()->contact(index)->as<DMRContact>();
    if ((!contact) || (!copy) || (contact->number() != _originalContacts[index].number)) {
      errMsg(err) << "Changing DM1702S contact types, numbers or order is not supported.";
      return false;
    }
    uint8_t contactType = 0;
    switch (contact->type()) {
    case DMRContact::PrivateCall: contactType=0x03; break;
    case DMRContact::GroupCall: contactType=0x04; break;
    case DMRContact::AllCall: contactType=0x05; break;
    }
    if (contactType != _originalContacts[index].type) {
      errMsg(err) << "Changing DM1702S contact types is not supported.";
      return false;
    }
    copy->setName(_originalContacts[index].name);
  }

  for (int index=0; index<normalized.rxGroupLists()->count(); ++index) {
    RXGroupList *group = normalized.rxGroupLists()->list(index);
    group->clear();
    group->setName(_originalRXGroupNames[index]);
    for (unsigned int id: _originalRXGroupMembers[index]) {
      DMRContact *contact = nullptr;
      for (int contactIndex=0; contactIndex<normalized.contacts()->count(); ++contactIndex) {
        auto *candidate = normalized.contacts()->contact(contactIndex)->as<DMRContact>();
        if (candidate && (DMRContact::GroupCall == candidate->type()) && (candidate->number() == id)) {
          contact = candidate;
          break;
        }
      }
      if (nullptr == contact) {
        errMsg(err) << "Cannot resolve original RX-group contact " << id << " in copied config.";
        return false;
      }
      group->addContact(contact);
    }
  }
  for (int index=0; index<_originalZoneNames.size(); ++index) {
    Zone *zone=normalized.zones()->zone(index);
    if (nullptr == zone) {
      errMsg(err) << "Cannot normalize DM1702S zone " << (index+1) << ".";
      return false;
    }
    zone->clear();
    zone->setName(_originalZoneNames[index]);
    for (unsigned int channelIndex: _originalZoneAMembers[index])
      zone->A()->add(normalized.channelList()->channel(static_cast<int>(channelIndex)));
    for (unsigned int channelIndex: _originalZoneBMembers[index])
      zone->B()->add(normalized.channelList()->channel(static_cast<int>(channelIndex)));
  }
  while (normalized.scanlists()->count() < _originalScanListNames.size())
    normalized.scanlists()->add(new ScanList());
  for (int index=0; index<_originalScanListNames.size(); ++index) {
    ScanList *copyList=normalized.scanlists()->scanlist(index);
    if (nullptr == copyList) {
      errMsg(err) << "Cannot normalize DM1702S scan list " << (index+1) << ".";
      return false;
    }
    copyList->clear();
    copyList->setName(_originalScanListNames[index]);
    for (unsigned int channelIndex: _originalScanListMembers[index])
      copyList->addChannel(normalized.channelList()->channel(static_cast<int>(channelIndex)));
    if (_originalScanListPrimaryIndexes[index] >= 0)
      copyList->setPrimaryChannel(normalized.channelList()->channel(_originalScanListPrimaryIndexes[index]));
    if (_originalScanListSecondaryIndexes[index] >= 0)
      copyList->setSecondaryChannel(normalized.channelList()->channel(_originalScanListSecondaryIndexes[index]));
  }
  for (int index=0; index<config->channelList()->count(); ++index) {
    Channel *sourceChannel=config->channelList()->channel(index);
    Channel *copyChannel=normalized.channelList()->channel(index);
    auto *sourceDMR=sourceChannel->as<DMRChannel>();
    auto *copyDMR=copyChannel->as<DMRChannel>();
    if (sourceDMR && copyDMR) {
      const int originalGPSIndex=_originalChannelGPSIndexes[index];
      copyDMR->setAPRS((originalGPSIndex < 0) ? nullptr :
          normalized.posSystems()->system(originalGPSIndex));
    }
    if (sourceChannel->scanList()) {
      int scanListIndex=-1;
      for (int candidate=0; candidate<config->scanlists()->count(); ++candidate)
        if (config->scanlists()->scanlist(candidate) == sourceChannel->scanList()) {
          scanListIndex=candidate;
          break;
        }
      if (scanListIndex < 0) {
        errMsg(err) << "DM1702S channel references an unknown scan list.";
        return false;
      }
    }
    const int originalScanListIndex=_originalChannelScanListIndexes[index];
    copyChannel->setScanList((originalScanListIndex < 0) ? nullptr :
        normalized.scanlists()->scanlist(originalScanListIndex));
    auto *source = config->channelList()->channel(index)->as<DMRChannel>();
    auto *copy = normalized.channelList()->channel(index)->as<DMRChannel>();
    if ((!source) || (!copy))
      continue;
    const int originalContactIndex=_originalChannelContactIndexes[index];
    DMRContact *originalContact = (originalContactIndex < 0) ? nullptr :
        normalized.contacts()->contact(originalContactIndex)->as<DMRContact>();
    copy->setContact(originalContact);
    if (!source->groupList())
      continue;
    int groupIndex = -1;
    for (int candidate=0; candidate<config->rxGroupLists()->count(); ++candidate)
      if (config->rxGroupLists()->list(candidate) == source->groupList()) {
        groupIndex = candidate;
        break;
      }
    if (groupIndex < 0) {
      errMsg(err) << "DM1702S channel references an unknown RX group list.";
      return false;
    }
    copy->setGroupList(normalized.rxGroupLists()->list(groupIndex));
  }
  while (normalized.posSystems()->count() > _originalGPSSystemNames.size())
    normalized.posSystems()->del(normalized.posSystems()->system(normalized.posSystems()->count()-1));
  while (normalized.zones()->count() > _originalZoneNames.size())
    normalized.zones()->del(normalized.zones()->zone(normalized.zones()->count()-1));
  while (normalized.scanlists()->count() > _originalScanListNames.size())
    normalized.scanlists()->del(normalized.scanlists()->scanlist(normalized.scanlists()->count()-1));
  for (int index=0; index<normalized.channelList()->count(); ++index)
    normalized.channelList()->channel(index)->setName(_originalChannelNames[index]);
  QString normalizedYAML;
  QTextStream normalizedStream(&normalizedYAML);
  if (!normalized.toYAML(normalizedStream, err))
    return false;
  if (normalizedYAML != _originalConfigYAML) {
    errMsg(err) << "Only supported DM1702S channel, contact, group, zone, scan list, GPS and radio settings can be edited.";
    return false;
  }

  QByteArray vfoPage=_pages.value(0x78);
  if ((vfoPage.size() != static_cast<int>(sectorSize)) ||
      (vfoPage.at(sectorSize-1) != static_cast<char>(0x78))) {
    errMsg(err) << "Missing or invalid DM1702S VFO page.";
    return false;
  }
  const DM1702SSettingsExtension *radioSettings=config->settings()->dm1702sExtension();
  if (nullptr == radioSettings) {
    errMsg(err) << "Missing DM1702S radio settings.";
    return false;
  }
  if (!writeBCD24Frequency(vfoPage, 0x0fa0, radioSettings->vfoARXFrequency(), err) ||
      !writeBCD24Frequency(vfoPage, 0x0fa4, radioSettings->vfoATXFrequency(), err) ||
      !writeBCD24Frequency(vfoPage, 0x0fd0, radioSettings->vfoBRXFrequency(), err) ||
      !writeBCD24Frequency(vfoPage, 0x0fd4, radioSettings->vfoBTXFrequency(), err))
    return false;

  std::vector<std::pair<uint8_t, size_t>> zoneRanges{{0x45, 0x0f0c}};
  for (uint8_t marker=0x46; marker<=0x56; ++marker)
    zoneRanges.emplace_back(marker, 0x0efc);
  QByteArray zoneData;
  if (!appendRanges(_pages, zoneRanges, zoneData, err))
    return false;
  if ((config->zones()->count() > 250) ||
      (0x10+static_cast<size_t>(config->zones()->count())*0x112 > static_cast<size_t>(zoneData.size()))) {
    errMsg(err) << "DM1702S zone data exceeds its page chain.";
    return false;
  }
  zoneData[0]=static_cast<char>(config->zones()->count());
  for (int zoneIndex=0; zoneIndex<config->zones()->count(); ++zoneIndex) {
    Zone *zone=config->zones()->zone(zoneIndex);
    const QByteArray name=zone->name().toLatin1();
    if (zone->name().isEmpty() || (QString::fromLatin1(name) != zone->name()) ||
        (name.size() > 16) || name.contains('\0') ||
        std::any_of(zone->name().cbegin(), zone->name().cend(), [](QChar character) {
          return !character.isPrint();
        }) || (zone->A()->count() > 64) || (zone->B()->count() > 64)) {
      errMsg(err) << "DM1702S zone names must be printable Latin-1 and each side limited to 64 channels.";
      return false;
    }
    const size_t offset=0x10+static_cast<size_t>(zoneIndex)*0x112;
    std::fill_n(zoneData.begin()+offset, 16, '\0');
    std::copy(name.cbegin(), name.cend(), zoneData.begin()+offset);
    zoneData[offset+0x10]=static_cast<char>(zone->A()->count());
    zoneData[offset+0x91]=static_cast<char>(zone->B()->count());
    for (int side=0; side<2; ++side) {
      ChannelRefList *members=(0 == side) ? zone->A() : zone->B();
      const size_t memberOffset=offset+((0 == side) ? 0x11 : 0x92);
      for (int member=0; member<members->count(); ++member) {
        Channel *channel=members->get(member)->as<Channel>();
        const int channelIndex=config->channelList()->indexOf(channel);
        if ((nullptr == channel) || (channelIndex < 0)) {
          errMsg(err) << "DM1702S zone references a channel outside the channel list.";
          return false;
        }
        const uint16_t channelNumber=static_cast<uint16_t>(channelIndex+1);
        zoneData[memberOffset+static_cast<size_t>(member)*2]=static_cast<char>(channelNumber & 0xff);
        zoneData[memberOffset+static_cast<size_t>(member)*2+1]=static_cast<char>(channelNumber >> 8);
      }
    }
  }

  std::vector<PageSlice> channelSlices;
  for (uint8_t marker=0x16; marker<=0x22; ++marker)
    channelSlices.push_back({marker, 0});
  QByteArray channelData;
  if (!appendPages(_pages, channelSlices, channelData, err))
    return false;
  for (int index=0; index<config->channelList()->count(); ++index) {
    auto *channel=config->channelList()->channel(index)->as<DMRChannel>();
    if (nullptr == channel)
      continue;
    unsigned int gpsPosition=0;
    if (channel->aprs()) {
      if (!channel->aprs()->is<DMRAPRSSystem>()) {
        errMsg(err) << "DM1702S DMR channels require a DMR GPS system.";
        return false;
      }
      int gpsIndex=-1;
      for (int candidate=0; candidate<config->posSystems()->count(); ++candidate)
        if (config->posSystems()->system(candidate) == channel->aprs()) {
          gpsIndex=candidate;
          break;
        }
      if ((gpsIndex < 0) || (gpsIndex >= 15)) {
        errMsg(err) << "DM1702S channel GPS system is not in the encodable system list.";
        return false;
      }
      gpsPosition=static_cast<unsigned int>(gpsIndex)+1;
    }
    const size_t offset=static_cast<size_t>(index)*channelRecordSize;
    DM1702SChannelElement element(reinterpret_cast<uint8_t *>(channelData.data()+offset));
    const int scanListIndex=config->scanlists()->indexOf(channel->scanList());
    if (channel->scanList()) {
      if ((scanListIndex < 0) || (scanListIndex >= 32)) {
        errMsg(err) << "DM1702S channel references an invalid scan list.";
        return false;
      }
      element.setScanListIndex(static_cast<unsigned int>(scanListIndex));
    } else {
      element.clearScanListIndex();
    }
    element.setDMRAPRSChannelIndex(gpsPosition);
  }

  constexpr size_t scanListRecordSize=0x39;
  constexpr size_t scanListMemberCount=15;
  const size_t originalScanListCount=_originalScanListNames.size();
  QByteArray scanListData;
  if (!appendRanges(_pages, {{0x13, 0x0720}}, scanListData, err))
    return false;
  for (int index=0; index<config->scanlists()->count(); ++index) {
    const size_t offset=static_cast<size_t>(index)*scanListRecordSize;
    if (offset+scanListRecordSize > static_cast<size_t>(scanListData.size())) {
      errMsg(err) << "DM1702S scan list data exceeds its page.";
      return false;
    }
    if (static_cast<size_t>(index) >= originalScanListCount) {
      std::fill_n(scanListData.begin()+offset, scanListRecordSize, '\0');
      scanListData[offset+0x0c]=0x03;
      scanListData[offset+0x0d]=0x03;
      scanListData[offset+0x0e]=0x06;
      scanListData[offset+0x10]=0x01;
      scanListData[offset+0x16]=0x0a;
      scanListData[offset+0x19]=0x01;
    }
    ScanList *scanList=config->scanlists()->scanlist(index);
    const QByteArray name=scanList->name().toLatin1();
    std::fill_n(scanListData.begin()+offset+1, 16, '\0');
    std::copy(name.cbegin(), name.cend(), scanListData.begin()+offset+1);
    std::fill_n(scanListData.begin()+offset+0x1b, scanListMemberCount*2, '\0');
    for (int member=0; member<scanList->count(); ++member) {
      const int channelIndex=config->channelList()->indexOf(scanList->channel(member));
      const uint16_t channelNumber=static_cast<uint16_t>(channelIndex+1);
      const size_t memberOffset=offset+0x1b+static_cast<size_t>(member)*2;
      scanListData[memberOffset]=static_cast<char>(channelNumber & 0xff);
      scanListData[memberOffset+1]=static_cast<char>(channelNumber >> 8);
    }
    auto priorityChannelNumber=[&](Channel *channel) -> int {
      if (nullptr == channel)
        return 0;
      const int channelIndex=config->channelList()->indexOf(channel);
      if ((channelIndex < 0) || (channelIndex >= 255))
        return -1;
      return channelIndex+1;
    };
    const int primary=priorityChannelNumber(scanList->primaryChannel());
    const int secondary=priorityChannelNumber(scanList->secondaryChannel());
    if ((primary < 0) || (secondary < 0)) {
      errMsg(err) << "DM1702S priority channels must reference one of the first 255 codeplug channels.";
      return false;
    }
    if (primary || secondary)
      scanListData[offset+0x0f]=0x02;
    else if ((static_cast<size_t>(index) < originalScanListCount) &&
             ((_originalScanListPrimaryIndexes[index] >= 0) ||
              (_originalScanListSecondaryIndexes[index] >= 0)))
      scanListData[offset+0x0f]=0;
    scanListData[offset+0x12]=static_cast<char>(primary);
    scanListData[offset+0x13]=static_cast<char>(secondary);
  }
  scanListData[0]=static_cast<char>(config->scanlists()->count());

  const std::vector<std::pair<uint8_t, size_t>> channelContactRanges{{0x27, 0x0800}};
  QByteArray channelContactData;
  if (!appendRanges(_pages, channelContactRanges, channelContactData, err))
    return false;
  for (int index=0; index<config->channelList()->count(); ++index) {
    auto *channel=config->channelList()->channel(index)->as<DMRChannel>();
    if (nullptr == channel)
      continue;
    const int originalContactIndex=_originalChannelContactIndexes[index];
    DMRContact *originalContact = (originalContactIndex < 0) ? nullptr :
        config->contacts()->contact(originalContactIndex)->as<DMRContact>();
    if (channel->contact() == originalContact)
      continue;

    unsigned int contactPosition=0;
    unsigned int contactType=0;
    if (channel->contact()) {
      if ((DMRContact::PrivateCall != channel->contact()->type()) &&
          (DMRContact::GroupCall != channel->contact()->type()) &&
          (DMRContact::AllCall != channel->contact()->type())) {
        errMsg(err) << "DM1702S TX contacts must be stored DMR contacts.";
        return false;
      }
      int contactIndex=-1;
      for (int candidate=0; candidate<config->contacts()->count(); ++candidate)
        if (config->contacts()->contact(candidate) == channel->contact()) {
          contactIndex=candidate;
          break;
        }
      if ((contactIndex < 0) || (contactIndex >= _originalContacts.size()) ||
          (contactIndex >= 0x0ffe) ||
          (std::numeric_limits<size_t>::max() == _originalContacts[contactIndex].dataOffset)) {
        errMsg(err) << "DM1702S TX contact is not present in the stored contact list.";
        return false;
      }
      contactPosition=static_cast<unsigned int>(contactIndex)+1;
      contactType=(DMRContact::PrivateCall == channel->contact()->type()) ? 0 : 1;
    }
    const size_t offset=static_cast<size_t>(index)*2;
    channelContactData[offset]=static_cast<char>(((contactPosition >> 8) << 4) | contactType);
    channelContactData[offset+1]=static_cast<char>(contactPosition & 0xff);
  }

  QByteArray channelNames;
  const std::vector<PageSlice> nameSlices{{0x24, 0}, {0x25, 3}, {0x26, 0}};
  if (!appendPages(_pages, nameSlices, channelNames, err))
    return false;
  for (int index=0; index<config->channelList()->count(); ++index) {
    const QString &name = config->channelList()->channel(index)->name();
    const QByteArray encodedName = name.toLatin1();
    if (name.isEmpty() || (encodedName.size() > static_cast<int>(channelNameSize)) ||
        (QString::fromLatin1(encodedName) != name) || encodedName.contains('\0')) {
      errMsg(err) << "DM1702S channel names must contain 1 to 11 Latin-1 bytes.";
      return false;
    }
    const size_t offset = static_cast<size_t>(index)*channelNameSize;
    std::fill_n(channelNames.begin()+offset, channelNameSize, '\0');
    std::copy(encodedName.cbegin(), encodedName.cend(), channelNames.begin()+offset);
  }

  const std::vector<std::pair<uint8_t, size_t>> contactSlices{
    {0x28, 0x0ff0}, {0x29, 0x0ff0}, {0x2a, 0x0ff0}, {0x2b, 0x0ff0}, {0x2c, 0x0ff0}
  };
  QByteArray contactData;
  if (!appendRanges(_pages, contactSlices, contactData, err))
    return false;
  for (int index=0; index<config->contacts()->count(); ++index) {
    const ContactRecord &record = _originalContacts[index];
    if (std::numeric_limits<size_t>::max() == record.dataOffset)
      continue;
    auto *contact = config->contacts()->contact(index)->as<DMRContact>();
    const QString &contactName = contact->name();
    if (contactName == record.name)
      continue;
    const QByteArray name = contactName.toLatin1();
    if ((QString::fromLatin1(name) != contactName) || (name.size() > 16) ||
        name.contains('\0') || name.contains(static_cast<char>(0xff))) {
      errMsg(err) << "DM1702S contact names must contain at most 16 Latin-1 bytes.";
      return false;
    }
    std::fill_n(contactData.begin()+record.dataOffset+2, 16, '\0');
    std::copy(name.cbegin(), name.cend(), contactData.begin()+record.dataOffset+2);
  }

  QByteArray rxListData;
  const std::vector<std::pair<uint8_t, size_t>> rxListRanges{{0x11, 0x0606}, {0x12, 0x073d}};
  if (!appendRanges(_pages, rxListRanges, rxListData, err))
    return false;
  for (int groupIndex=0; groupIndex<config->rxGroupLists()->count(); ++groupIndex) {
    const size_t offset=_originalRXGroupOffsets[groupIndex];
    const size_t memberData=offset+13;
    const size_t memberBytes=32*3;
    if ((memberData+memberBytes) > static_cast<size_t>(rxListData.size())) {
      errMsg(err) << "DM1702S RX group record is outside its page chain.";
      return false;
    }
    std::fill_n(rxListData.begin()+memberData, memberBytes, '\0');
    RXGroupList *group=config->rxGroupLists()->list(groupIndex);
    for (int member=0; member<group->count(); ++member) {
      const uint32_t id=group->contact(member)->number();
      const size_t idOffset=memberData+static_cast<size_t>(member)*3;
      rxListData[idOffset]=static_cast<char>(id&0xff);
      rxListData[idOffset+1]=static_cast<char>((id>>8)&0xff);
      rxListData[idOffset+2]=static_cast<char>((id>>16)&0xff);
    }
  }

  auto settingsPage = _pages.find(0x04);
  if ((settingsPage == _pages.end()) || (settingsPage->size() != static_cast<int>(sectorSize)) ||
      (settingsPage->at(sectorSize-1) != static_cast<char>(0x04))) {
    errMsg(err) << "Missing or invalid DM1702S settings page.";
    return false;
  }
  const QByteArray date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toLatin1();
  if (10 != date.size()) {
    errMsg(err) << "Cannot encode the current date for DM1702S.";
    return false;
  }

  const QByteArray radioAlias = radioSettings->radioAlias().toLatin1();
  if ((QString::fromLatin1(radioAlias) != radioSettings->radioAlias()) ||
      (radioAlias.size() > 16) || radioAlias.contains('\0') ||
      radioAlias.contains(static_cast<char>(0xff))) {
    errMsg(err) << "DM1702S radio alias must contain at most 16 Latin-1 bytes.";
    return false;
  }

  if (!writePages(_pages, nameSlices, channelNames, err))
    return false;
  if (!writePages(_pages, channelSlices, channelData, err))
    return false;
  if (!writeRanges(_pages, channelContactRanges, channelContactData, err))
    return false;
  if (!writeRanges(_pages, {{0x13, 0x0720}}, scanListData, err))
    return false;
  if (!writeRanges(_pages, zoneRanges, zoneData, err))
    return false;
  if (!writeRanges(_pages, contactSlices, contactData, err))
    return false;
  if (!writeRanges(_pages, rxListRanges, rxListData, err))
    return false;
  _pages[0x78]=vfoPage;
  std::memcpy(settingsPage->data()+0x10, date.constData(), date.size());
  std::memset(settingsPage->data()+0x1d0, 0, 16);
  std::memcpy(settingsPage->data()+0x1d0, radioAlias.constData(), radioAlias.size());
  settingsPage->data()[0x44] = static_cast<char>((static_cast<uint8_t>(settingsPage->at(0x44)) & 0xfe) |
                                                  (radioSettings->aprsEnabled() ? 0x01 : 0x00));
  settingsPage->data()[0x700] = static_cast<char>((static_cast<uint8_t>(settingsPage->at(0x700)) & 0xfe) |
                                                   (DM1702SSettingsExtension::DataTransmitServer::MOTOIP ==
                                                    radioSettings->dataTransmitServer() ? 0x01 : 0x00));
  for (int index=0; index<config->posSystems()->count(); ++index) {
    const QByteArray name=config->posSystems()->system(index)->name().toLatin1();
    const size_t offset=0x500+static_cast<size_t>(index)*0x10;
    std::memset(settingsPage->data()+offset, 0, 9);
    std::memcpy(settingsPage->data()+offset, name.constData(), static_cast<size_t>(name.size()));
  }
  settingsPage->data()[0x600]=static_cast<char>(config->posSystems()->count());
  return true;
}

bool
DM1702SCodeplug::index(Config *config, Context &ctx, const ErrorStack &err) const {
  if (nullptr == config) {
    errMsg(err) << "Cannot index DM1702S codeplug without a configuration.";
    return false;
  }
  for (int index=0; index<config->channelList()->count(); ++index) {
    if (! ctx.add(config->channelList()->channel(index), static_cast<unsigned int>(index))) {
      errMsg(err) << "Cannot index DM1702S channel " << (index+1) << ".";
      return false;
    }
  }
  return true;
}