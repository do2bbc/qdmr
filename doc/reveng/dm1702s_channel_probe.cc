#include "config.hh"
#include "dm1702s_codeplug.hh"
#include "errorstack.hh"

#include <QCoreApplication>
#include <QByteArray>
#include <QLocale>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr size_t sectorSize = 0x1000;

std::vector<std::string> parseCsvRow(const std::string &line) {
  std::vector<std::string> fields;
  std::string field;
  bool quoted = false;
  for (size_t index=0; index<line.size(); ++index) {
    const char character = line[index];
    if ('"' == character) {
      if (quoted && (index+1 < line.size()) && ('"' == line[index+1])) {
        field.push_back('"');
        ++index;
      } else {
        quoted = !quoted;
      }
    } else if ((',' == character) && (!quoted)) {
      fields.push_back(field);
      field.clear();
    } else if ('\r' != character) {
      field.push_back(character);
    }
  }
  fields.push_back(field);
  return fields;
}

bool readFile(const char *path, std::vector<uint8_t> &data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "Cannot open " << path << '\n';
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return true;
}

long long frequencyHz(const std::string &text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (std::string::npos == first)
    return 0;
  const size_t last = text.find_last_not_of(" \t\r\n");
  std::string normalized = text.substr(first, last-first+1);
  std::replace(normalized.begin(), normalized.end(), ',', '.');
  bool valid = false;
  const double mhz = QLocale::c().toDouble(QString::fromStdString(normalized), &valid);
  if (!valid)
    return 0;
  return std::llround(mhz * 1000000.0);
}
}

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  if (3 != argc) {
    std::cerr << "Usage: dm1702s_channel_probe <raw-image> <channel-csv>\n";
    return 2;
  }

  std::vector<uint8_t> rawImage;
  if (!readFile(argv[1], rawImage) || (rawImage.size() % sectorSize)) {
    std::cerr << "Raw image must contain complete 4 KiB sectors.\n";
    return 1;
  }

  std::map<uint8_t, std::vector<uint8_t>> sectors;
  for (size_t offset=0; offset<rawImage.size(); offset+=sectorSize) {
    const uint8_t marker = rawImage[offset+sectorSize-1];
    if ((0 == marker) || (0xff == marker))
      continue;
    sectors[marker] = std::vector<uint8_t>(rawImage.begin()+offset,
                                           rawImage.begin()+offset+sectorSize);
  }

  DM1702SCodeplug codeplug;
  ErrorStack error;
  for (const auto &sector: sectors) {
    const QByteArray page(reinterpret_cast<const char *>(sector.second.data()),
                          static_cast<int>(sector.second.size()));
    if (!codeplug.setPage(sector.first, page, error)) {
      std::cerr << error.format().toStdString() << '\n';
      return 1;
    }
  }
  Config config;
  if (!codeplug.decode(&config, error)) {
    std::cerr << error.format().toStdString() << '\n';
    return 1;
  }
  const size_t channelCount = static_cast<size_t>(config.channelList()->count());

  std::ifstream csv(argv[2]);
  std::string line;
  if (!std::getline(csv, line)) {
    std::cerr << "CSV is empty.\n";
    return 1;
  }
  const std::vector<std::string> headers = parseCsvRow(line);
  auto column = [&](const std::string &name) {
    auto found = std::find(headers.begin(), headers.end(), name);
    return (found == headers.end()) ? headers.size() : static_cast<size_t>(found-headers.begin());
  };
  const size_t aliasColumn = column("Alias");
  const size_t typeColumn = column("Type");
  const size_t rxColumn = column("RX Freq");
  const size_t txColumn = column("TX Freq");
  const size_t numberColumn = column("CH.");
  if ((aliasColumn == headers.size()) || (typeColumn == headers.size()) ||
      (rxColumn == headers.size()) ||
      (txColumn == headers.size()) || (numberColumn == headers.size())) {
    std::cerr << "CSV must contain CH., Type, Alias, RX Freq, and TX Freq columns.\n";
    return 1;
  }

  size_t rows = 0;
  size_t namesMatched = 0;
  size_t typesMatched = 0;
  size_t rawRxMatched = 0;
  size_t rawTxMatched = 0;
  size_t rxMatched = 0;
  size_t txMatched = 0;
  while (std::getline(csv, line)) {
    const std::vector<std::string> fields = parseCsvRow(line);
    if (fields.size() <= std::max({aliasColumn, typeColumn, rxColumn, txColumn, numberColumn}))
      continue;
    ++rows;

    const std::string &alias = fields[aliasColumn];
    size_t channelNumber = 0;
    try {
      channelNumber = std::stoul(fields[numberColumn]);
    } catch (...) {
      continue;
    }
    if (alias.empty() || !channelNumber || (channelNumber > channelCount))
      continue;
    const Channel *channel = config.channelList()->channel(channelNumber-1);
    const bool expectedDMR = ("1" == fields[typeColumn]);
    if (expectedDMR == channel->is<DMRChannel>())
      ++typesMatched;
    if (channel->name().toStdString() != alias)
      continue;
    ++namesMatched;

    const long long expectedRx = frequencyHz(fields[rxColumn]);
    const long long expectedTx = frequencyHz(fields[txColumn]);
    if (expectedRx && (std::llabs(static_cast<long long>(channel->rxFrequency().inHz())-
                                  expectedRx) <= 10))
      ++rawRxMatched;
    if (expectedTx && (std::llabs(static_cast<long long>(channel->txFrequency().inHz())-
                                  expectedTx) <= 10))
      ++rawTxMatched;
    if (expectedRx && (std::llabs(static_cast<long long>(channel->rxFrequency().inHz())-
                                  expectedRx) <= 10))
      ++rxMatched;
    if (expectedTx && (std::llabs(static_cast<long long>(channel->txFrequency().inHz())-
                                  expectedTx) <= 10))
      ++txMatched;
  }

  std::cout << "Image channels=" << channelCount << " CSV rows=" << rows
            << " names matched=" << namesMatched << " types matched=" << typesMatched
            << " raw RX/TX matches=" << rawRxMatched << '/' << rawTxMatched
            << " qdmr channels decoded=" << channelCount
            << " RX matches=" << rxMatched << " TX matches=" << txMatched << '\n';
  const DM1702SSettingsExtension *radioSettings=config.settings()->dm1702sExtension();
  std::cout << "VFO A RX/TX=" << radioSettings->vfoARXFrequency().inHz() << '/'
            << radioSettings->vfoATXFrequency().inHz()
            << " VFO B RX/TX=" << radioSettings->vfoBRXFrequency().inHz() << '/'
            << radioSettings->vfoBTXFrequency().inHz() << '\n';
  std::cout << "Zones=" << config.zones()->count();
  for (int index=0; index<config.zones()->count(); ++index) {
    const Zone *zone=config.zones()->zone(index);
    std::cout << " [" << zone->name().toStdString() << " A=" << zone->A()->count()
              << " B=" << zone->B()->count() << ']';
  }
  std::cout << " GPS=" << config.posSystems()->count();
  for (int index=0; index<config.posSystems()->count(); ++index)
    std::cout << " [" << config.posSystems()->system(index)->name().toStdString() << ']';
  for (int index=0; index<config.channelList()->count(); ++index) {
    const auto *channel=config.channelList()->channel(index)->as<DMRChannel>();
    if (channel && channel->aprs())
      std::cout << " CH" << (index+1) << " GPS=" << channel->aprs()->name().toStdString();
  }
  std::cout << '\n';
  return ((rows == channelCount) && (namesMatched == rows) && (typesMatched == rows) &&
      (rxMatched == rows) && (txMatched == rows)) ? 0 : 3;
}