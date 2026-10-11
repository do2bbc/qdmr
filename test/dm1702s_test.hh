#ifndef DM1702S_TEST_HH
#define DM1702S_TEST_HH

#include <QObject>

class DM1702STest : public QObject
{
  Q_OBJECT

private slots:
  void testChannelDecode();
  void testCallsignDatabaseEncoding();
  void testCallsignDatabaseDecoding();
  void testRadioIdCSVImport();
  void testSettingsYAMLSerialization();
  void testEncodingRequiresDecodedSource();
  void testRadioRegistrationIsExplicit();
};

#endif // DM1702S_TEST_HH