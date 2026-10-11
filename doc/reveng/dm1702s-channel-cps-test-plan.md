# DM-1702S Channel CPS Differential Tests

Use `/mnt/nas_fs/do2bbc.data` as the read-only baseline. Do not save over it and
do not upload test variants to the radio. In CPS, use Save As for every row
below. Change exactly one field relative to that variant's stated parent. Keep
each variant as a separate `.data` file so its bytes can be compared with its
parent.

The values below reflect the `DO2BBC TS2` channel shown in the CPS screenshot.
If the baseline shows a different value, record the actual baseline value and
change only that field. If a required alternative does not exist, skip that
row and note why; do not add other settings just to create an alternative.

## Shared Channel Fields

| ID | Field | Baseline | Test value |
| --- | --- | --- | --- |
| C01 | Channel alias | `DO2BBC TS2` | `DO2BBC TST` |
| C02 | RX frequency | `439.8250 MHz` | `439.8260 MHz` |
| C03 | TX frequency | `430.5000 MHz` | `430.5010 MHz` |

## Digital Channel Fields

Keep the channel type Digital for these variants. Start each variant from the
read-only baseline, not from the previous variant.

| ID | Field | Baseline | Test value |
| --- | --- | --- | --- |
| D01 | Auto Start Scan | Off | On |
| D02 | Scan List | None | Another existing scan list |
| D03 | Emergency Alarm Indication | Off | On |
| D04 | Emergency Call Indication | Off | On |
| D05 | Emergency Alarm Ack | Off | On, after D04 |
| D06 | Emergency System | None | An existing system |
| D07 | RX Only | Off | On |
| D08 | Lone Worker | Off | On |
| D09 | Power | Low | High |
| D10 | VOX | Off | On |
| D11 | Talk Around Status | Talk Around On | Another available status |
| D12 | Talk Around | Off | On |
| D13 | Digital TX Admit | Always Allow | Channel Free |
| D14 | Private Call Confirmed | Not Confirm | Confirm |
| D15 | Short Data Significance Bit | Nonconfirming Mode | Confirming Mode |
| D16 | Slot Operation | Slot 2 | Slot 1 |
| D17 | Color Code | 1 | 2 |
| D18 | Encrypt Key | None | An existing key; leave Encrypt off |
| D19 | Encrypt | Off | On, using the key selected in a separate variant |
| D20 | TX Contact Name | Regional | Another existing contact |
| D21 | RX Group List | Hotspot | Another existing group list |
| D22 | GPS System | Brandmeis | Another existing GPS system |
| D23 | Double Capacity Mode | Off | On |

The CPS requires Emergency Call Indication before Emergency Alarm Ack. The
already saved files `D04-emergency-ack.data` and `D05-emergency-call.data` use
the earlier labels in reverse; interpret these two files by their byte changes
and save order, not by their filenames.

For D19, start from the D18 variant with a valid key selected and change only
Encrypt to On. Compare D18 with the original baseline to identify the key-index
change, then compare D19 with D18 to isolate the Encrypt bit. If no key is
available, skip D18 and D19.

## Analog Channel Fields

First make one `analog-base.data` copy from the baseline and change only
Channel Type from Digital to Analog. Then make every analog test variant from
that analog base, changing just one analog field per file.

| ID | Field | Baseline | Test value |
| --- | --- | --- | --- |
| A01 | CTCSS/CDCS Decode | None | CTCSS 88.5 Hz |
| A02 | CTCSS/CDCS Encode | None | CTCSS 88.5 Hz |
| A03 | Channel Spacing | Narrow | Wide |
| A04 | Squelch | General | One adjacent available setting |
| A05 | Analog TX Admit | Always Allow | Channel Free |
| A06 | PTT ID Display | Off | On |
| A07 | Signaling Type | None | An available non-None type |
| A08 | RX Signaling System | None | An existing system |
| A09 | TX Signaling System | None | An existing system |
| A10 | RX Squelch Mode | Carrier | Another available mode |
| A11 | Auto Reset Time | 10 s | 20 s |
| A12 | Step Frequency | 25 kHz | 12.5 kHz |

Save files with names that include the ID, for example `dmr-D17-color-code.data`
or `analog-A02-encode-tone.data`. Please send the variants in ID order; that
lets us map each binary difference to exactly one CPS setting.