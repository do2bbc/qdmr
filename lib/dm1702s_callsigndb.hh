#ifndef DM1702S_CALLSIGNDB_HH
#define DM1702S_CALLSIGNDB_HH

#include "errorstack.hh"
#include "userdatabase.hh"

#include <QByteArray>
#include <QVector>

class DM1702SCallsignDB
{
public:
  static constexpr unsigned int maxEntries() { return 6750; }
  static constexpr uint32_t address() { return 0x246000; }
  static constexpr unsigned int imageSize() { return 0x0b66c0; }

  static bool encode(const QVector<UserDatabase::User> &users, QByteArray &data,
                     const ErrorStack &err=ErrorStack());
  static bool decodeToJson(const QByteArray &data, QByteArray &json,
                           const ErrorStack &err=ErrorStack());
};

#endif // DM1702S_CALLSIGNDB_HH