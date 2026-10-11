# How to reverse engineer code-plugs?

This question is certainly not easy to answer. It heavily depends on the particular device. In 
general, reverse engineering the code-plug requires two steps. First, the communication protocol
between the host and the device must be known. This step can be skipped if the protocol is already
known (e.g., for Radioddity, Baofeng, Tytera, Retevis or AnyTone devices) or when the protocol is 
actually an industry standard (e.g., DFU). 

Once you know how the host communicates with the device, the actual code-plug can be tackled: The 
general approach is a so-called differential analysis. That is, using the original CPS, you start 
with a compact code-plug that covers all features for the radio. Then you change only ONE thing 
using the original CPS and compare the two results. 

Sometimes, the manufacturer is nice to you and the binary code-plug saved by the original CPS is 
basically a one-to-one image of the memory content written to the device. This is true for the 
Radioddity GD-77 code-plugs and to some lesser extend for the RD-5R, TyT MD-UV390 code-plugs. 

Sometimes, however, the manufacturer CPS saves something that has basically nothing to do with 
what is written to the device. For example the AnyTone D-878UV. In these cases, the content 
being written to the device must be captured. This can be done using wireshark on a linux 
host. That is, fire up a virtual machine running the original CPS. Start wireshark on the host 
system and monitor the USB interface using *usbmon*. Then, write a small script that extracts 
the actual data written to the device from the capture files. These can then be compared to 
perform the differential analysis. An example script including captured traffic and extracted 
data can be found in the anytone directory.

## Retevis RT72 / Baofeng DM-1702

Public references:

* [qdmr RT72/DM-1702 support issue](https://github.com/hmatuschek/qdmr/issues/181), including USB read/write captures and a CPS codeplug
* [qdmr RT72/DM-1702 protocol notes](https://github.com/hmatuschek/qdmr/tree/dm1702/doc/reveng/baofeng/dm1702)
* [MD1702-tools data maps](https://github.com/OK2MOP/MD1702-tools/blob/master/DM1702_data_maps.py)
* [MD1702-tools codeplug notes](https://github.com/OK2MOP/MD1702-tools/blob/master/doc/codeplug.md)
* [MD1702-tools parser](https://github.com/OK2MOP/MD1702-tools/blob/master/DM1702_codeplug.py)
* [Baofeng DM-X project](https://gitlab.com/jimiwills/baofeng-dm-x), which provides sample codeplugs but no field-level parser

The public data map is useful for locating page chains and byte ranges, but it is not a complete
field specification. Its comments explicitly mark several ranges as approximate or dynamic. qdmr
therefore edits only fields with a verified layout; the original pages for other areas remain
untouched and unsupported semantic changes are rejected.

The RT72/DM-1702 family has multiple firmware/CPS branches. Public reports distinguish RT72 CPS
for `L05.01.005` from MD CPS for `V02.02.014`. The RT72 USB capture in issue #181 reports device ID
`DM1702S`, uses the same `PSEARCH`/`PASSSTA`/`SYSINFO` protocol, and its CPS image has the same
4-KiB marker-page layout as the local `do2bbc*.data` images. The separate
`D1.00.01.001 e.data` bundled with DM-1702Pro CPS does not contain marker `0x16`; it is not treated
as that same page-image format.

| Area | Public map | qdmr status |
| --- | --- | --- |
| Channel records and names | Markers `0x16`-`0x22`, `0x24`-`0x26` | RT72-specific channel type, RX/TX frequencies, RX-only, Lone Worker (bit 3 of byte `0x18`), Auto Start Scan (bit 7 of byte `0x19`), emergency flags/system, and names decoded/editable |
| Contact records and allocation map | Marker `0x0b`, markers `0x28`-`0x2c` | Decoded; contact aliases editable |
| Channel TX-contact map | Marker `0x27`, first `0x800` bytes | Decoded and editable for stored contacts |
| LContact contact database | Address `0x246000`; 16-byte header, up to 6,750 108-byte records, 37 records per 4-KiB page | Read to qdmr-compatible `users.json`, import of RadioID JSON/CSV lists, and upload are implemented. The filtered 5,785-entry RadioID CSV was hardware-written and read back exactly; over-width text fields are limited to their fixed widths |
| RX groups | Marker `0x11` first `0x606` bytes and marker `0x12` first `0x73d` bytes | Decoded; group membership editable |
| Scan lists | Marker `0x13`; 32 records of `0x39` bytes, active count in byte 0, names at `+1`, up to 15 one-based channel slots from `+0x1b`; priority channels at `+0x12/+0x13` | Names, membership, priority channels and channel default-list links decoded/editable; new lists appendable (up to 32); TX/revert and timing fields remain untouched |
| Radio settings | Marker `0x04` | Radio alias, APRS enable, data-transmit server, and two 16-byte boot messages at `0x180`/`0x1c0` decoded/editable |
| VFO records | Marker `0x78`; VFO A RX/TX at `0xfa0`/`0xfa4`, VFO B RX/TX at `0xfd0`/`0xfd4`; each is 24-bit little-endian BCD in kHz | Four RX/TX frequencies decoded and editable |
| Systems, lone worker and privacy | Marker `0x11`, ranges `0x606`-`0x6ab`, `0x700`-`0x7ff`, `0x800`-`0xbff` | Ranges located; field layouts are incomplete or marked uncertain |
| Zones | Markers `0x45`-`0x56`; `0x112`-byte records with a 16-byte name, then A/B counts and up to 64 little-endian one-based channel indices per side | Names and A/B membership decoded; appending and editing supported, removal rejected |
| GPS systems | Marker `0x04`, 16-byte records from `0x500`, active count at `0x600`; 9-byte Latin-1 name | Names/count decoded; append and rename supported, other record bytes preserved |
| GPS channel assignment | DMR channel record byte `0x20`, low nibble is a one-based GPS-system index | Decoded and editable |
| Other GPS/config fields | Marker `0x04`, remaining settings bytes | Preserved; undocumented fields are not edited |
| Message templates | Marker `0x0a`; reference reader uses a count and `0x81`-byte records | Public reader exists; qdmr has no matching configuration model |

The VFO frequencies are stored as six-digit BCD values scaled in kHz. The four locations are
independent RX/TX values; the supplied CPS edits updated both directions together.

The TX-contact map uses one 2-byte entry per channel. The upper nibble of the first byte and the
second byte form the 12-bit, one-based contact index; index zero and `0xfff` mean no contact. The
low nibble records private-versus-non-private call type. qdmr refuses to write contacts synthesized
only to satisfy an RX group, because they have no contact record in the radio. RT72 channel type is
bit 6 of record byte `0x18`; this interpretation matches the `Type` column for all 25 local CPS
channels. Scan-list memberships are decoded conservatively; priority channels at `+0x12/+0x13` are
decoded and editable, while TX/revert and timing fields remain unmapped.

For DM-1702S DMR channels, byte `0x1b` contains Emergency Alarm Indication in bit 7, Emergency Call
Indication in bit 6, Emergency Alarm Ack in bit 5, and a one-based Emergency System number in bits
3-0 (zero means none). This mapping is confirmed by cumulative CPS variants; bit 4 is preserved.
The CPS clears Emergency Call Indication when RX-only is enabled; qdmr rejects that combination
rather than silently dropping either setting. The cumulative CPS channel tests are documented in
`dm1702s-channel-cps-test-plan.md`.