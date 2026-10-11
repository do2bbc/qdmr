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
}

DM1702SCodeplug::DM1702SCodeplug(QObject *parent)
  : Codeplug(parent), _pages(), _pageAddresses(), _originalChannelNames(), _originalRadioAlias(),
    _originalAprsEnabled(false),
    _originalDataTransmitServer(DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData),
    _originalConfigYAML()
{
  addImage(QStringLiteral("DM1702S"));
}

void
DM1702SCodeplug::clearPages() {
  _pages.clear();
  _pageAddresses.clear();
  _originalChannelNames.clear();
  _originalContacts.clear();
  _originalRXGroupOffsets.clear();
  _originalRXGroupNames.clear();
  _originalRXGroupMembers.clear();
  _originalRadioAlias.clear();
  _originalAprsEnabled = false;
  _originalDataTransmitServer = DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData;
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

  QByteArray contactMap, contactData, rxListData;
  std::vector<std::pair<uint8_t, size_t>> contactSlices;
  for (uint8_t marker=0x28; marker<=0x2c; ++marker)
    contactSlices.emplace_back(marker, 0x0ff0);
  if (!appendRanges(_pages, {{0x0b, 0x0fff}}, contactMap, err) ||
      !appendRanges(_pages, contactSlices, contactData, err) ||
      !appendRanges(_pages, {{0x11, 0x0606}, {0x12, 0x073d}}, rxListData, err))
    return false;
  if ((contactMap.size() < 0x74) || (contactData.size() < 800*0x18)) {
    errMsg(err) << "DM1702S contact data is truncated.";
    return false;
  }

  config->contacts()->clear();
  config->rxGroupLists()->clear();
  _originalContacts.clear();
  QMap<unsigned int, DMRContact *> groupContacts;
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
  Context context(config);
  for (size_t index=0; index<channelCount; ++index) {
    const QString name = readChannelName(channelNames, index);
    if (name.isEmpty()) {
      errMsg(err) << "DM1702S channel " << (index+1) << " has no name.";
      return false;
    }

    std::array<uint8_t, channelRecordSize> record{};
    std::copy_n(reinterpret_cast<const uint8_t *>(channelData.constData())+
                  index*channelRecordSize, channelRecordSize, record.begin());
    DM32UVCodeplug::ChannelElement element(record.data());
    element.setName(name);
    Channel *channel = element.decode(context, err);
    if (nullptr == channel) {
      errMsg(err) << "Cannot decode DM1702S channel " << (index+1) << ".";
      return false;
    }
    config->channelList()->add(channel);
    if (channel->is<DMRChannel>()) {
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
    }
    context.add(channel, static_cast<unsigned int>(index));
    _originalChannelNames.append(name);
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
  if ((config->rxGroupLists()->count() != _originalRXGroupOffsets.size()) ||
      (_originalRXGroupOffsets.size() != _originalRXGroupNames.size()) ||
      (_originalRXGroupOffsets.size() != _originalRXGroupMembers.size())) {
    errMsg(err) << "Adding or removing DM1702S RX group lists is not supported yet.";
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
  for (int index=0; index<config->channelList()->count(); ++index) {
    auto *source = config->channelList()->channel(index)->as<DMRChannel>();
    auto *copy = normalized.channelList()->channel(index)->as<DMRChannel>();
    if ((!source) || (!copy) || (!source->groupList()))
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
  for (int index=0; index<normalized.channelList()->count(); ++index)
    normalized.channelList()->channel(index)->setName(_originalChannelNames[index]);
  QString normalizedYAML;
  QTextStream normalizedStream(&normalizedYAML);
  if (!normalized.toYAML(normalizedStream, err))
    return false;
  if (normalizedYAML != _originalConfigYAML) {
    errMsg(err) << "Only DM1702S channel names, RX-group members and radio settings can be edited.";
    return false;
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

  const DM1702SSettingsExtension *radioSettings = config->settings()->dm1702sExtension();
  if (nullptr == radioSettings) {
    errMsg(err) << "Missing DM1702S radio settings.";
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
  if (!writeRanges(_pages, rxListRanges, rxListData, err))
    return false;
  std::memcpy(settingsPage->data()+0x10, date.constData(), date.size());
  std::memset(settingsPage->data()+0x1d0, 0, 16);
  std::memcpy(settingsPage->data()+0x1d0, radioAlias.constData(), radioAlias.size());
  settingsPage->data()[0x44] = static_cast<char>((static_cast<uint8_t>(settingsPage->at(0x44)) & 0xfe) |
                                                  (radioSettings->aprsEnabled() ? 0x01 : 0x00));
  settingsPage->data()[0x700] = static_cast<char>((static_cast<uint8_t>(settingsPage->at(0x700)) & 0xfe) |
                                                   (DM1702SSettingsExtension::DataTransmitServer::MOTOIP ==
                                                    radioSettings->dataTransmitServer() ? 0x01 : 0x00));
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