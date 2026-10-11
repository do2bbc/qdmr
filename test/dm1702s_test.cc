#include "dm1702s_test.hh"

#include "channel.hh"
#include "config.hh"
#include "dm1702s.hh"
#include "dm1702s_codeplug.hh"
#include "dm1702s_interface.hh"
#include "gpssystem.hh"
#include "zone.hh"

#include <QDate>
#include <QTest>
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
  QVERIFY2(codeplug.setPage(0x04, page(0x04), err), qPrintable(err.format()));
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
      data[0x18]=static_cast<char>(0x46);
      data[0x19]=1;
      data[0x1a]=static_cast<char>(0x10);
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
  QCOMPARE(config.channelList()->channel(0)->as<DMRChannel>()->contact()->number(), 60u);
  DMRChannel *decodedChannel=config.channelList()->channel(0)->as<DMRChannel>();
  QCOMPARE(decodedChannel->admit(), DMRChannel::Admit::ColorCode);
  QCOMPARE(decodedChannel->timeSlot(), DMRChannel::TimeSlot::TS2);
  QCOMPARE(decodedChannel->colorCode(), 1u);
  QVERIFY(decodedChannel->extended()->loneWorker());
  QVERIFY(decodedChannel->extended()->privateCallConfirm());
  QVERIFY(decodedChannel->extended()->dataConfirm());
  QVERIFY(decodedChannel->extended()->dcdm());
  QVERIFY(decodedChannel->extended()->talkaround());
  auto *gpsSystem=new DMRAPRSSystem(QStringLiteral("Test GPS"));
  config.posSystems()->add(gpsSystem);
  QVERIFY(decodedChannel->setAPRS(gpsSystem));
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
  config.settings()->dm1702sExtension()->setRadioAlias(QStringLiteral("DO2BBC Boris Tes"));
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
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0]), 2);
  QCOMPARE(codeplug.page(0x45).mid(0x122, 9), QByteArray("QDMR-TEST", 9));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0x132]), 1);
  QCOMPARE(codeplug.page(0x45).mid(0x133, 2), QByteArray::fromHex("0100"));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x45)[0x1b3]), 1);
  QCOMPARE(codeplug.page(0x45).mid(0x1b4, 2), QByteArray::fromHex("0100"));
  QCOMPARE(codeplug.page(0x27).left(2), QByteArray::fromHex("0102"));
  DM1702SCodeplug reloaded;
  for (uint8_t marker: codeplug.pageMarkers())
    QVERIFY2(reloaded.setPage(marker, codeplug.page(marker), err), qPrintable(err.format()));
  Config roundTrip;
  QVERIFY2(reloaded.decode(&roundTrip, err), qPrintable(err.format()));
  QCOMPARE(roundTrip.scanlists()->count(), 1);
  QCOMPARE(roundTrip.scanlists()->scanlist(0)->name(), QStringLiteral("Test Scan"));
  QCOMPARE(roundTrip.scanlists()->scanlist(0)->count(), 1);
  QCOMPARE(roundTrip.channelList()->channel(0)->scanList(), roundTrip.scanlists()->scanlist(0));
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

  const QByteArray encodedNames = codeplug.page(0x24);
  config.channelList()->channel(0)->setRXFrequency(Frequency::fromMHz(438.9125));
  QVERIFY(!codeplug.encode(&config, Codeplug::Flags(), err));
  QCOMPARE(codeplug.page(0x24), encodedNames);
}

void
DM1702STest::testEncodingRequiresDecodedSource() {
  DM1702SCodeplug codeplug;
  Config config;
  ErrorStack err;
  QVERIFY(! codeplug.encode(&config, Codeplug::Flags(), err));
}

void
DM1702STest::testRadioRegistrationIsExplicit() {
  const RadioInfo info = DM1702S::defaultRadioInfo();
  QCOMPARE(info.key(), QStringLiteral("dm1702s"));
  QCOMPARE(info.id(), RadioInfo::DM1702S);
  QVERIFY(info.interfaceMatches(DM1702SInterface::interfaceInfo()));
  QVERIFY(!DM1702SInterface::interfaceInfo().isSave());
}

QTEST_GUILESS_MAIN(DM1702STest)