#include "dm1702s_test.hh"

#include "channel.hh"
#include "config.hh"
#include "dm1702s.hh"
#include "dm1702s_callsigndb.hh"
#include "dm1702s_codeplug.hh"
#include "dm1702s_interface.hh"
#include "gpssystem.hh"
#include "radiolimits.hh"
#include "userdatabase.hh"
#include "zone.hh"

#include <QDate>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include <algorithm>

namespace {
QByteArray page(uint8_t marker, char fill=0) {
  QByteArray data(0x1000, fill);
  data[0xfff] = static_cast<char>(marker);
  return data;
}
}

void
DM1702STest::testChannelDecode() {
  DM1702SCodeplug codeplug;
  ErrorStack err;
  QByteArray settingsPage=page(0x04);
  std::copy_n("Dies Geraet", 11, settingsPage.begin()+0x180);
  std::copy_n("Gehoert DO2BBC", 14, settingsPage.begin()+0x1c0);
  QVERIFY2(codeplug.setPage(0x04, settingsPage, err), qPrintable(err.format()));
  QByteArray vfoPage=page(0x78, 0);
  vfoPage[0x0fa0]=0x00; vfoPage[0x0fa1]=0x48; vfoPage[0x0fa2]=0x43;
  vfoPage[0x0fa4]=0x00; vfoPage[0x0fa5]=0x48; vfoPage[0x0fa6]=0x43;
  vfoPage[0x0fd0]=0x25; vfoPage[0x0fd1]=0x28; vfoPage[0x0fd2]=0x43;
  vfoPage[0x0fd4]=0x25; vfoPage[0x0fd5]=0x28; vfoPage[0x0fd6]=0x43;
  QVERIFY2(codeplug.setPage(0x78, vfoPage, err), qPrintable(err.format()));

  QByteArray contactMap=page(0x0b, static_cast<char>(0xff));
  contactMap[0]=1;
  contactMap[2]=1;
  contactMap[0x10]=static_cast<char>(0xfc);
  QVERIFY2(codeplug.setPage(0x0b, contactMap, err), qPrintable(err.format()));

  QByteArray rxLists=page(0x11, 0);
  rxLists[0x0f]=0;
  rxLists[0x10]=1;
  const QByteArray groupName("Test Group");
  std::copy(groupName.cbegin(), groupName.cend(), rxLists.begin()+0x11);
  rxLists[0x1c]=60;
  rxLists[0x1f]=8;
  QVERIFY2(codeplug.setPage(0x11, rxLists, err), qPrintable(err.format()));
  QVERIFY2(codeplug.setPage(0x12, page(0x12, 0), err), qPrintable(err.format()));
  QByteArray scanLists=page(0x13, 0);
  scanLists[0]=1;
  const QByteArray scanListName("Test Scan");
  std::copy(scanListName.cbegin(), scanListName.cend(), scanLists.begin()+1);
  scanLists[0x1b]=1;
  QVERIFY2(codeplug.setPage(0x13, scanLists, err), qPrintable(err.format()));
  for (uint8_t marker=0x45; marker<=0x56; ++marker) {
    QByteArray data=page(marker, 0);
    if (0x45 == marker) {
      data[0]=1;
      const QByteArray zoneName("Test Zone");
      std::copy(zoneName.cbegin(), zoneName.cend(), data.begin()+0x10);
      data[0x20]=1;
      data[0x21]=1;
    }
    QVERIFY2(codeplug.setPage(marker, data, err), qPrintable(err.format()));
  }
  QByteArray channelContacts=page(0x27, static_cast<char>(0xff));
  channelContacts[0]=0;
  channelContacts[1]=1;
  QVERIFY2(codeplug.setPage(0x27, channelContacts, err), qPrintable(err.format()));
  for (uint8_t marker=0x28; marker<=0x2c; ++marker) {
    QByteArray data=page(marker, static_cast<char>(0xff));
    if (0x28 == marker) {
      std::fill_n(data.begin(), 0x30, '\0');
      const QByteArray contactName("AF German");
      std::copy(contactName.cbegin(), contactName.cend(), data.begin()+2);
      data[19]=60;
      data[20]=0;
      data[21]=0;
      data[22]=4;
      const QByteArray secondContactName("Second");
      std::copy(secondContactName.cbegin(), secondContactName.cend(), data.begin()+0x1a);
      data[0x18+19]=61;
      data[0x18+20]=0;
      data[0x18+21]=0;
      data[0x18+22]=4;
    }
    QVERIFY2(codeplug.setPage(marker, data, err), qPrintable(err.format()));
  }

  for (uint8_t marker=0x16; marker<=0x22; ++marker) {
    QByteArray data = page(marker, static_cast<char>(0xff));
    if (0x16 == marker) {
      data[0] = 1;
      data[1] = 0;
      const uint8_t rx[4] = {0x00, 0x25, 0x98, 0x43};
      const uint8_t tx[4] = {0x00, 0x00, 0x05, 0x43};
      for (int i=0; i<4; ++i) {
        data[0x10+i] = static_cast<char>(rx[i]);
        data[0x14+i] = static_cast<char>(tx[i]);
      }
      for (int i=0x18; i<0x20; ++i)
        data[i] = 0;
      data[0x18]=static_cast<char>(0x42);
      data[0x19]=1;
      data[0x1a]=static_cast<char>(0x10);
      data[0x1b]=static_cast<char>(0xe1);
      data[0x1d]=static_cast<char>(0x30);
      data[0x1e]=static_cast<char>(0x31);
      data[0x20]=0;
      data[0x21]=1;
      data[0x27]=0;
      data[0x2b]=static_cast<char>(0x40);
    }
    QVERIFY2(codeplug.setPage(marker, data, err), qPrintable(err.format()));
  }

  for (uint8_t marker=0x24; marker<=0x26; ++marker) {
    QByteArray data = page(marker, 0);
    if (0x24 == marker) {
      data[0] = 'T'; data[1] = 'e'; data[2] = 's'; data[3] = 't';
    }
    QVERIFY2(codeplug.setPage(marker, data, err), qPrintable(err.format()));
  }

  Config config;
  QVERIFY2(codeplug.decode(&config, err), qPrintable(err.format()));
  QCOMPARE(config.channelList()->count(), 1);
  QVERIFY(config.channelList()->channel(0)->is<DMRChannel>());
  QCOMPARE(config.channelList()->channel(0)->name(), QStringLiteral("Test"));
  QCOMPARE(config.channelList()->channel(0)->rxFrequency(), Frequency::fromHz(439825000));
  QCOMPARE(config.channelList()->channel(0)->txFrequency(), Frequency::fromHz(430500000));
  QCOMPARE(config.settings()->dm1702sExtension()->vfoARXFrequency(), Frequency::fromHz(434800000));
  QCOMPARE(config.settings()->dm1702sExtension()->vfoATXFrequency(), Frequency::fromHz(434800000));
  QCOMPARE(config.settings()->dm1702sExtension()->vfoBRXFrequency(), Frequency::fromHz(432825000));
  QCOMPARE(config.settings()->dm1702sExtension()->vfoBTXFrequency(), Frequency::fromHz(432825000));
  QCOMPARE(config.channelList()->channel(0)->power(), Channel::Power::High);
  QCOMPARE(config.scanlists()->count(), 1);
  QCOMPARE(config.scanlists()->scanlist(0)->name(), QStringLiteral("Test Scan"));
  QCOMPARE(config.scanlists()->scanlist(0)->count(), 1);
  QCOMPARE(config.scanlists()->scanlist(0)->channel(0), config.channelList()->channel(0));
  QCOMPARE(config.channelList()->channel(0)->scanList(), config.scanlists()->scanlist(0));
  QCOMPARE(config.zones()->count(), 1);
  QCOMPARE(config.zones()->zone(0)->name(), QStringLiteral("Test Zone"));
  QCOMPARE(config.zones()->zone(0)->A()->count(), 1);
  QCOMPARE(config.zones()->zone(0)->A()->get(0), config.channelList()->channel(0));
  QCOMPARE(config.contacts()->count(), 3);
  QCOMPARE(config.contacts()->contact(0)->name(), QStringLiteral("AF German"));
  QCOMPARE(config.settings()->boot()->message1(), QStringLiteral("Dies Geraet"));
  QCOMPARE(config.settings()->boot()->message2(), QStringLiteral("Gehoert DO2BBC"));
  QCOMPARE(config.channelList()->channel(0)->as<DMRChannel>()->contact()->number(), 60u);
  DMRChannel *decodedChannel=config.channelList()->channel(0)->as<DMRChannel>();
  QCOMPARE(decodedChannel->admit(), DMRChannel::Admit::ColorCode);
  QCOMPARE(decodedChannel->timeSlot(), DMRChannel::TimeSlot::TS2);
  QCOMPARE(decodedChannel->colorCode(), 1u);
  QVERIFY(!decodedChannel->extended()->loneWorker());
  QVERIFY(!decodedChannel->dm1702sChannelExtension()->autoStartScan());
  QVERIFY(decodedChannel->extended()->privateCallConfirm());
  QVERIFY(decodedChannel->extended()->dataConfirm());
  QVERIFY(decodedChannel->extended()->dcdm());
  QVERIFY(decodedChannel->extended()->talkaround());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyAlarmIndication());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyCallIndication());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyAlarmAck());
  QCOMPARE(decodedChannel->dm1702sChannelExtension()->emergencySystem(),
           DM1702SChannelExtension::EmergencySystem::System1);
  auto *gpsSystem=new DMRAPRSSystem(QStringLiteral("Test GPS"));
  config.posSystems()->add(gpsSystem);
  QVERIFY(decodedChannel->setAPRS(gpsSystem));
  auto *newScanList=new ScanList(QStringLiteral("qdmr test"));
  newScanList->addChannel(config.channelList()->channel(0));
  newScanList->setPrimaryChannel(config.channelList()->channel(0));
  config.scanlists()->add(newScanList, -1, false);
  auto *testZone=new Zone(QStringLiteral("QDMR-TEST"));
  testZone->A()->add(config.channelList()->channel(0));
  testZone->B()->add(config.channelList()->channel(0));
  config.zones()->add(testZone, -1, false);
  QCOMPARE(config.rxGroupLists()->count(), 1);
  QCOMPARE(config.rxGroupLists()->list(0)->name(), QStringLiteral("Test Group"));
  QCOMPARE(config.rxGroupLists()->list(0)->count(), 2);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(0)->number(), 60u);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(1)->number(), 8u);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(1)->name(), QStringLiteral("Group Call"));
  config.contacts()->contact(0)->setName(QStringLiteral("michi do2ith"));
  QVERIFY(config.rxGroupLists()->list(0)->remContact(0));

  config.channelList()->channel(0)->setName(QStringLiteral("Renamed"));
  QVERIFY(decodedChannel->setRXFrequency(Frequency::fromHz(439826000)));
  QVERIFY(decodedChannel->setTXFrequency(Frequency::fromHz(430501000)));
  decodedChannel->extended()->enableLoneWorker(true);
  decodedChannel->dm1702sChannelExtension()->enableAutoStartScan(true);
  decodedChannel->extended()->enableLoneWorker(true);
  decodedChannel->dm1702sChannelExtension()->enableEmergencyAlarmIndication(false);
  decodedChannel->dm1702sChannelExtension()->enableEmergencyCallIndication(true);
  decodedChannel->dm1702sChannelExtension()->enableEmergencyAlarmAck(false);
  decodedChannel->dm1702sChannelExtension()->setEmergencySystem(
      DM1702SChannelExtension::EmergencySystem::System2);
  config.settings()->dm1702sExtension()->setRadioAlias(QStringLiteral("DO2BBC Boris Tes"));
  config.settings()->boot()->setMessage1(QStringLiteral("Boot line one"));
  config.settings()->boot()->setMessage2(QStringLiteral("Boot line two"));
  config.settings()->dm1702sExtension()->setAprsEnabled(true);
  config.settings()->dm1702sExtension()->setDataTransmitServer(
      DM1702SSettingsExtension::DataTransmitServer::MOTOIP);
  config.settings()->dm1702sExtension()->setVfoARXFrequency(Frequency::fromHz(145500000));
  config.settings()->dm1702sExtension()->setVfoATXFrequency(Frequency::fromHz(145500000));
  config.settings()->dm1702sExtension()->setVfoBRXFrequency(Frequency::fromHz(433500000));
  config.settings()->dm1702sExtension()->setVfoBTXFrequency(Frequency::fromHz(433500000));
  QVERIFY(config.channelList()->channel(0)->as<DMRChannel>()->setContact(
      config.contacts()->contact(1)->as<DMRContact>()));
  QVERIFY2(codeplug.encode(&config, Codeplug::Flags(), err), qPrintable(err.format()));
  QCOMPARE(codeplug.page(0x78).mid(0x0fa0, 7), QByteArray::fromHex("00551400005514"));
  QCOMPARE(codeplug.page(0x78).mid(0x0fd0, 7), QByteArray::fromHex("00354300003543"));
  QCOMPARE(codeplug.page(0x04).mid(0x500, 9), QByteArray("Test GPS\0", 9));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x600]), 1);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16)[0x20]) & 0x0f, 1);
  QCOMPARE(codeplug.page(0x16).mid(0x10, 4), QByteArray::fromHex("00269843"));
  QCOMPARE(codeplug.page(0x16).mid(0x14, 4), QByteArray::fromHex("00010543"));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16)[0x18]), static_cast<uint8_t>(0x4a));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16)[0x19]), static_cast<uint8_t>(0x81));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0]), 2);
  QCOMPARE(codeplug.page(0x45).mid(0x122, 9), QByteArray("QDMR-TEST", 9));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0x132]), 1);
  QCOMPARE(codeplug.page(0x45).mid(0x133, 2), QByteArray::fromHex("0100"));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0x1b3]), 1);
  QCOMPARE(codeplug.page(0x45).mid(0x1b4, 2), QByteArray::fromHex("0100"));
  QCOMPARE(codeplug.page(0x27).left(2), QByteArray::fromHex("0102"));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x13)[0]), 2);
  QCOMPARE(codeplug.page(0x13).mid(0x3a, 9), QByteArray("qdmr test", 9));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x13)[0x48]), 2);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x13)[0x4b]), 1);
  QCOMPARE(codeplug.page(0x13).mid(0x54, 2), QByteArray::fromHex("0100"));
  DM1702SCodeplug reloaded;
  for (uint8_t marker: codeplug.pageMarkers())
    QVERIFY2(reloaded.setPage(marker, codeplug.page(marker), err), qPrintable(err.format()));
  Config roundTrip;
  QVERIFY2(reloaded.decode(&roundTrip, err), qPrintable(err.format()));
  QCOMPARE(roundTrip.settings()->boot()->message1(), QStringLiteral("Boot line one"));
  QCOMPARE(roundTrip.settings()->boot()->message2(), QStringLiteral("Boot line two"));
  QCOMPARE(roundTrip.channelList()->channel(0)->as<DMRChannel>()
               ->dm1702sChannelExtension()->emergencySystem(),
           DM1702SChannelExtension::EmergencySystem::System2);
  QVERIFY(!roundTrip.channelList()->channel(0)->as<DMRChannel>()
              ->dm1702sChannelExtension()->emergencyAlarmIndication());
  QVERIFY(roundTrip.channelList()->channel(0)->as<DMRChannel>()
              ->dm1702sChannelExtension()->emergencyCallIndication());
  QVERIFY(!roundTrip.channelList()->channel(0)->as<DMRChannel>()
              ->dm1702sChannelExtension()->emergencyAlarmAck());
  QCOMPARE(roundTrip.channelList()->channel(0)->rxFrequency(), Frequency::fromHz(439826000));
  QCOMPARE(roundTrip.channelList()->channel(0)->txFrequency(), Frequency::fromHz(430501000));
  QVERIFY(roundTrip.channelList()->channel(0)->rxOnly() == false);
  QVERIFY(roundTrip.channelList()->channel(0)->as<DMRChannel>()
              ->dm1702sChannelExtension()->autoStartScan());
  QVERIFY(roundTrip.channelList()->channel(0)->as<DMRChannel>()->extended()->loneWorker());
  QCOMPARE(roundTrip.scanlists()->count(), 2);
  QCOMPARE(roundTrip.scanlists()->scanlist(0)->name(), QStringLiteral("Test Scan"));
  QCOMPARE(roundTrip.scanlists()->scanlist(0)->count(), 1);
  QCOMPARE(roundTrip.channelList()->channel(0)->scanList(), roundTrip.scanlists()->scanlist(0));
  QCOMPARE(roundTrip.scanlists()->scanlist(1)->name(), QStringLiteral("qdmr test"));
  QCOMPARE(roundTrip.scanlists()->scanlist(1)->count(), 1);
  QCOMPARE(roundTrip.scanlists()->scanlist(1)->channel(0), roundTrip.channelList()->channel(0));
  QCOMPARE(roundTrip.scanlists()->scanlist(1)->primaryChannel(), roundTrip.channelList()->channel(0));
  QVERIFY(roundTrip.scanlists()->del(roundTrip.scanlists()->scanlist(1)));
  QVERIFY2(reloaded.encode(&roundTrip, Codeplug::Flags(), err), qPrintable(err.format()));
  QCOMPARE(static_cast<uint8_t>(reloaded.page(0x13)[0]), 1);
  QCOMPARE(roundTrip.posSystems()->count(), 1);
  QCOMPARE(roundTrip.posSystems()->system(0)->name(), QStringLiteral("Test GPS"));
  QCOMPARE(roundTrip.channelList()->channel(0)->as<DMRChannel>()->aprs(),
           roundTrip.posSystems()->system(0));
  QCOMPARE(roundTrip.zones()->count(), 2);
  QCOMPARE(roundTrip.zones()->zone(1)->name(), QStringLiteral("QDMR-TEST"));
  QCOMPARE(roundTrip.zones()->zone(1)->A()->count(), 1);
  QCOMPARE(roundTrip.zones()->zone(1)->B()->count(), 1);
  QCOMPARE(codeplug.page(0x11).mid(0x1c, 3), QByteArray::fromHex("080000"));
  QCOMPARE(codeplug.page(0x24).left(8), QByteArray("Renamed\0", 8));
  QCOMPARE(codeplug.page(0x28).mid(2, 16), QByteArray("michi do2ith\0\0\0\0", 16));
  QCOMPARE(codeplug.page(0x04).mid(0x10, 10),
           QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toLatin1());
  QCOMPARE(codeplug.page(0x04).mid(0x1d0, 16), QByteArray("DO2BBC Boris Tes", 16));
  QCOMPARE(codeplug.page(0x04).mid(0x180, 16), QByteArray("Boot line one\0\0", 16));
  QCOMPARE(codeplug.page(0x04).mid(0x1c0, 16), QByteArray("Boot line two\0\0", 16));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16).at(0x1b)), static_cast<uint8_t>(0x42));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x44]) & 0x01, 1);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x700]) & 0x01, 1);
  const QByteArray encodedChannelContacts=codeplug.page(0x27);
  QVERIFY(config.channelList()->channel(0)->as<DMRChannel>()->setContact(
      config.contacts()->contact(2)->as<DMRContact>()));
  QVERIFY(!codeplug.encode(&config, Codeplug::Flags(), err));
  QCOMPARE(codeplug.page(0x27), encodedChannelContacts);
  QVERIFY(config.channelList()->channel(0)->as<DMRChannel>()->setContact(
      config.contacts()->contact(1)->as<DMRContact>()));

  config.settings()->dm1702sExtension()->setAprsEnabled(false);
  config.settings()->dm1702sExtension()->setDataTransmitServer(
      DM1702SSettingsExtension::DataTransmitServer::HyteraDefinedData);
  QVERIFY2(codeplug.encode(&config, Codeplug::Flags(), err), qPrintable(err.format()));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x44]) & 0x01, 0);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x700]) & 0x01, 0);

  const QByteArray encodedChannelPage=codeplug.page(0x16);
  config.channelList()->channel(0)->as<DMRChannel>()->setColorCode(2);
  QVERIFY(!codeplug.encode(&config, Codeplug::Flags(), err));
  QCOMPARE(codeplug.page(0x16), encodedChannelPage);
  config.channelList()->channel(0)->as<DMRChannel>()->setColorCode(1);
  QVERIFY(decodedChannel->setRXOnly(true));
  const QByteArray beforeInvalidEmergencyCombination=codeplug.page(0x16);
  QVERIFY(!codeplug.encode(&config, Codeplug::Flags(), err));
  QCOMPARE(codeplug.page(0x16), beforeInvalidEmergencyCombination);
  decodedChannel->dm1702sChannelExtension()->enableEmergencyCallIndication(false);
  QVERIFY2(codeplug.encode(&config, Codeplug::Flags(), err), qPrintable(err.format()));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16)[0x18]) & 0x20, 0x20);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x16)[0x1b]), 0x02);
  DM1702SCodeplug rxOnlyReloaded;
  for (uint8_t marker: codeplug.pageMarkers())
    QVERIFY2(rxOnlyReloaded.setPage(marker, codeplug.page(marker), err), qPrintable(err.format()));
  Config rxOnlyRoundTrip;
  QVERIFY2(rxOnlyReloaded.decode(&rxOnlyRoundTrip, err), qPrintable(err.format()));
  QVERIFY(rxOnlyRoundTrip.channelList()->channel(0)->rxOnly());
  QVERIFY(!rxOnlyRoundTrip.channelList()->channel(0)->as<DMRChannel>()
              ->dm1702sChannelExtension()->emergencyCallIndication());
}

void
DM1702STest::testEncodingRequiresDecodedSource() {
  DM1702SCodeplug codeplug;
  Config config;
  ErrorStack err;
  QVERIFY(! codeplug.encode(&config, Codeplug::Flags(), err));
}

void
DM1702STest::testCallsignDatabaseEncoding() {
  QVector<UserDatabase::User> users;
  for (unsigned int index=1; index<=38; ++index) {
    UserDatabase::User user;
    user.id=index;
    user.call=QStringLiteral("TEST%1").arg(index);
    users.append(user);
  }
  users[0].id=2624888;
  users[0].call=QStringLiteral("DH2VH");
  users[0].name=QStringLiteral("Volker");
  users[0].surname=QStringLiteral("Voigt");
  users[0].city=QStringLiteral("Bocholt");
  users[0].country=QStringLiteral("Germany");

  QByteArray data;
  ErrorStack err;
  QVERIFY2(DM1702SCallsignDB::encode(users, data, err), qPrintable(err.format()));
  QCOMPARE(data.size(), static_cast<int>(DM1702SCallsignDB::imageSize()));
  QCOMPARE(data.left(2), QByteArray::fromHex("2600"));
  QCOMPARE(data.mid(16, 4), QByteArray::fromHex("03780d28"));
  QCOMPARE(data.mid(20, 8), QByteArray("DH2VH\0\0\0", 8));
  QCOMPARE(data.mid(28, 16), QByteArray("Volker\0\0\0\0\0\0\0\0\0\0", 16));
  QCOMPARE(data.mid(44, 16), QByteArray("Voigt\0\0\0\0\0\0\0\0\0\0\0", 16));
  QCOMPARE(data.mid(60, 16), QByteArray("Bocholt\0\0\0\0\0\0\0\0\0", 16));
  QCOMPARE(data.mid(92, 16), QByteArray("Germany\0\0\0\0\0\0\0\0\0", 16));
  QCOMPARE(data.mid(0x1000, 4), QByteArray::fromHex("03260000"));
  QCOMPARE(data.mid(16+37*108, 0x1000-(16+37*108)).count('\0'),
           0x1000-(16+37*108));
}

void
DM1702STest::testCallsignDatabaseDecoding() {
  QVector<UserDatabase::User> users(1);
  users[0].id=2624888;
  users[0].call=QStringLiteral("DH2VH");
  users[0].name=QStringLiteral("Volker");
  users[0].surname=QStringLiteral("Voigt");
  users[0].city=QStringLiteral("Bocholt");
  users[0].country=QStringLiteral("Germany");

  QByteArray image, json;
  ErrorStack err;
  QVERIFY2(DM1702SCallsignDB::encode(users, image, err), qPrintable(err.format()));
  QVERIFY2(DM1702SCallsignDB::decodeToJson(image, json, err), qPrintable(err.format()));
  const QJsonArray decoded=QJsonDocument::fromJson(json).object()
      .value(QStringLiteral("users")).toArray();
  QCOMPARE(decoded.size(), 1);
  const QJsonObject user=decoded.first().toObject();
  QCOMPARE(user.value(QStringLiteral("id")).toInt(), 2624888);
  QCOMPARE(user.value(QStringLiteral("callsign")).toString(), QStringLiteral("DH2VH"));
  QCOMPARE(user.value(QStringLiteral("fname")).toString(), QStringLiteral("Volker"));
  QCOMPARE(user.value(QStringLiteral("surname")).toString(), QStringLiteral("Voigt"));
  QCOMPARE(user.value(QStringLiteral("city")).toString(), QStringLiteral("Bocholt"));
  QCOMPARE(user.value(QStringLiteral("country")).toString(), QStringLiteral("Germany"));
}

void
DM1702STest::testRadioIdCSVImport() {
  QTemporaryDir dataHome;
  QVERIFY(dataHome.isValid());
  const QByteArray previousDataHome=qgetenv("XDG_DATA_HOME");
  const bool hadDataHome=qEnvironmentVariableIsSet("XDG_DATA_HOME");
  qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
  QStandardPaths::setTestModeEnabled(true);

  {
    const QString appData=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(QDir().mkpath(appData));
    QSaveFile cache(appData+QStringLiteral("/user.json"));
    QVERIFY(cache.open(QIODevice::WriteOnly));
    QVERIFY(cache.write(R"({"users":[{"id":1,"callsign":"CACHE"}]})") > 0);
    QVERIFY(cache.commit());

    UserDatabase database(false, 36500);
    const QByteArray csv(
        "Radio ID,Callsign,Name,Nickname,City,State,Country,Remarks\r\n"
        "2624401,DF1VB,Jochen,Jochen,Dortmund,Nordrhein-Westfalen,Germany,\r\n"
      "2624444,DF1VB,\"Jochen, Jr.\",\"Jochen, Jr.\",\"Aachen, Mitte\",NRW,Germany,\"Test remark\"\r\n");
    QTemporaryFile csvFile(QDir::tempPath()+QStringLiteral("/qdmr-radioid-XXXXXX.csv"));
    QVERIFY(csvFile.open());
    QCOMPARE(csvFile.write(csv), static_cast<qint64>(csv.size()));
    csvFile.flush();

    QVERIFY(database.load(csvFile.fileName()));
    QCOMPARE(database.count(), 2);
    QCOMPARE(database.user(0).id, 2624401u);
    QCOMPARE(database.user(0).surname, QStringLiteral("Jochen"));
    QCOMPARE(database.user(1).name, QStringLiteral("Jochen, Jr."));
    QCOMPARE(database.user(1).surname, QStringLiteral("Jochen, Jr."));
    QCOMPARE(database.user(1).city, QStringLiteral("Aachen, Mitte"));
    QCOMPARE(database.user(1).comment, QStringLiteral("Test remark"));
  }

  QStandardPaths::setTestModeEnabled(false);
  if (hadDataHome)
    qputenv("XDG_DATA_HOME", previousDataHome);
  else
    qunsetenv("XDG_DATA_HOME");
}

void
DM1702STest::testSettingsYAMLSerialization() {
  Config source;
  auto *settings=new DM1702SSettingsExtension();
  source.settings()->setDM1702SExtension(settings);
  settings->setRadioAlias(QStringLiteral("DM1702S YAML"));
  settings->setVfoARXFrequency(Frequency::fromHz(145500000));
  settings->setVfoATXFrequency(Frequency::fromHz(145500000));
  settings->setVfoBRXFrequency(Frequency::fromHz(433500000));
  settings->setVfoBTXFrequency(Frequency::fromHz(433500000));
  auto *channel=new DMRChannel();
  channel->setName(QStringLiteral("Emergency Test"));
  channel->setRXFrequency(Frequency::fromHz(439825000));
  channel->setTXFrequency(Frequency::fromHz(430500000));
  channel->extended()->enableLoneWorker(true);
  channel->setDM1702SChannelExtension(new DM1702SChannelExtension());
  channel->dm1702sChannelExtension()->enableEmergencyAlarmIndication(true);
  channel->dm1702sChannelExtension()->enableEmergencyAlarmAck(true);
  channel->dm1702sChannelExtension()->enableEmergencyCallIndication(true);
  channel->dm1702sChannelExtension()->enableAutoStartScan(true);
  channel->dm1702sChannelExtension()->setEmergencySystem(
      DM1702SChannelExtension::EmergencySystem::System4);
  source.channelList()->add(channel);

  ErrorStack err;
  QString yaml;
  QTextStream yamlStream(&yaml);
  QVERIFY2(source.toYAML(yamlStream, err), qPrintable(err.format()));

  QTemporaryFile file;
  QVERIFY(file.open());
  QTextStream fileStream(&file);
  fileStream << yaml;
  fileStream.flush();
  file.close();

  Config decoded;
  QVERIFY2(decoded.readYAML(file.fileName(), err), qPrintable(err.format()));
  QVERIFY(decoded.settings()->dm1702sExtension());
  QCOMPARE(decoded.settings()->dm1702sExtension()->radioAlias(), QStringLiteral("DM1702S YAML"));
  QCOMPARE(decoded.settings()->dm1702sExtension()->vfoARXFrequency(), Frequency::fromHz(145500000));
  QCOMPARE(decoded.settings()->dm1702sExtension()->vfoATXFrequency(), Frequency::fromHz(145500000));
  QCOMPARE(decoded.settings()->dm1702sExtension()->vfoBRXFrequency(), Frequency::fromHz(433500000));
  QCOMPARE(decoded.settings()->dm1702sExtension()->vfoBTXFrequency(), Frequency::fromHz(433500000));
  DMRChannel *decodedChannel=decoded.channelList()->channel(0)->as<DMRChannel>();
  QVERIFY(decodedChannel);
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyAlarmIndication());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyAlarmAck());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->emergencyCallIndication());
  QVERIFY(decodedChannel->dm1702sChannelExtension()->autoStartScan());
  QCOMPARE(decodedChannel->dm1702sChannelExtension()->emergencySystem(),
           DM1702SChannelExtension::EmergencySystem::System4);
  QVERIFY(decodedChannel->extended()->loneWorker());
}

void
DM1702STest::testRadioRegistrationIsExplicit() {
  const RadioInfo info = DM1702S::defaultRadioInfo();
  QCOMPARE(info.key(), QStringLiteral("dm1702s"));
  QCOMPARE(info.id(), RadioInfo::DM1702S);
  QVERIFY(info.interfaceMatches(DM1702SInterface::interfaceInfo()));
  QVERIFY(!DM1702SInterface::interfaceInfo().isSave());
  DM1702S radio;
  QVERIFY(radio.limits().hasCallSignDB());
  QVERIFY(radio.limits().callSignDBImplemented());
  QCOMPARE(radio.limits().numCallSignDBEntries(), 6750u);
}

QTEST_GUILESS_MAIN(DM1702STest)