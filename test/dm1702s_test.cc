#include "dm1702s_test.hh"

#include "channel.hh"
#include "config.hh"
#include "dm1702s.hh"
#include "dm1702s_codeplug.hh"
#include "dm1702s_interface.hh"

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

  QByteArray contactMap=page(0x0b, static_cast<char>(0xff));
  contactMap[0]=1;
  contactMap[2]=1;
  contactMap[0x10]=static_cast<char>(0xfe);
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
  for (uint8_t marker=0x28; marker<=0x2c; ++marker) {
    QByteArray data=page(marker, static_cast<char>(0xff));
    if (0x28 == marker) {
      std::fill_n(data.begin(), 0x18, '\0');
      const QByteArray contactName("AF German");
      std::copy(contactName.cbegin(), contactName.cend(), data.begin()+2);
      data[19]=60;
      data[20]=0;
      data[21]=0;
      data[22]=4;
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
  QVERIFY(config.channelList()->channel(0)->is<FMChannel>());
  QCOMPARE(config.channelList()->channel(0)->name(), QStringLiteral("Test"));
  QCOMPARE(config.channelList()->channel(0)->rxFrequency(), Frequency::fromHz(439825000));
  QCOMPARE(config.channelList()->channel(0)->txFrequency(), Frequency::fromHz(430500000));
  QCOMPARE(config.contacts()->count(), 2);
  QCOMPARE(config.contacts()->contact(0)->name(), QStringLiteral("AF German"));
  QCOMPARE(config.rxGroupLists()->count(), 1);
  QCOMPARE(config.rxGroupLists()->list(0)->name(), QStringLiteral("Test Group"));
  QCOMPARE(config.rxGroupLists()->list(0)->count(), 2);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(0)->number(), 60u);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(1)->number(), 8u);
  QCOMPARE(config.rxGroupLists()->list(0)->contact(1)->name(), QStringLiteral("Group Call"));
  QVERIFY(config.rxGroupLists()->list(0)->remContact(0));

  config.channelList()->channel(0)->setName(QStringLiteral("Renamed"));
  config.settings()->dm1702sExtension()->setRadioAlias(QStringLiteral("DO2BBC Boris Tes"));
  config.settings()->dm1702sExtension()->setAprsEnabled(true);
  config.settings()->dm1702sExtension()->setDataTransmitServer(
      DM1702SSettingsExtension::DataTransmitServer::MOTOIP);
  QVERIFY2(codeplug.encode(&config, Codeplug::Flags(), err), qPrintable(err.format()));
  QCOMPARE(codeplug.page(0x11).mid(0x1c, 3), QByteArray::fromHex("080000"));
  QCOMPARE(codeplug.page(0x24).left(8), QByteArray("Renamed\0", 8));
  QCOMPARE(codeplug.page(0x04).mid(0x10, 10),
           QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")).toLatin1());
  QCOMPARE(codeplug.page(0x04).mid(0x1d0, 16), QByteArray("DO2BBC Boris Tes", 16));
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x44]) & 0x01, 1);
  QCOMPARE(static_cast<uint8_t>(codeplug.page(0x04)[0x700]) & 0x01, 1);

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