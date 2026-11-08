# Initialized NPU data — en7523_heimgard_cx30

Base address used for EN7523 SRAM-C is `0x1e900000`. This base is not assumed to be identical on AN7552/AN7581/AN7583.

This file is a 32-bit little-endian word view, **not** a recovered C structure. Strings, raw floats, integers, pointers, and packed flags may overlap word boundaries.

| Offset | 32-bit little-endian value | Heuristic class |
|---|---|---|
| `0x0000` | `0x0019000a` | scalar or packed data |
| `0x0004` | `0x000005dc` | scalar or packed data |
| `0x0008` | `0x000005dc` | scalar or packed data |
| `0x000c` | `0x00000000` | zero |
| `0x0010` | `0x00000000` | zero |
| `0x0014` | `0x00000000` | zero |
| `0x0018` | `0x0000ffff` | scalar or packed data |
| `0x001c` | `0x00000001` | scalar or packed data |
| `0x0020` | `0x00000001` | scalar or packed data |
| `0x0024` | `0x84007060` | firmware address |
| `0x0028` | `0x84007078` | firmware address |
| `0x002c` | `0x840070a8` | firmware address |
| `0x0030` | `0x840070ac` | firmware address |
| `0x0034` | `0x840070b0` | firmware address |
| `0x0038` | `0x840070d8` | firmware address |
| `0x003c` | `0x840070e8` | firmware address |
| `0x0040` | `0x84007100` | firmware address |
| `0x0044` | `0x84007110` | firmware address |
| `0x0048` | `0x84007134` | firmware address |
| `0x004c` | `0x84007144` | firmware address |
| `0x0050` | `0x84007168` | firmware address |
| `0x0054` | `0x8400718c` | firmware address |
| `0x0058` | `0x84007190` | firmware address |
| `0x005c` | `0x840071b4` | firmware address |
| `0x0060` | `0x840071d4` | firmware address |
| `0x0064` | `0x00000000` | zero |
| `0x0068` | `0x840071e4` | firmware address |
| `0x006c` | `0x840071f0` | firmware address |
| `0x0070` | `0x84007204` | firmware address |
| `0x0074` | `0x84007320` | firmware address |
| `0x0078` | `0x84007348` | firmware address |
| `0x007c` | `0x00000000` | zero |
| `0x0080` | `0x84007370` | firmware address |
| `0x0084` | `0x84007398` | firmware address |
| `0x0088` | `0x840073c8` | firmware address |
| `0x008c` | `0x840073e8` | firmware address |
| `0x0090` | `0x84007448` | firmware address |
| `0x0094` | `0x840074c8` | firmware address |
| `0x0098` | `0x840074f4` | firmware address |
| `0x009c` | `0x84007508` | firmware address |
| `0x00a0` | `0x00000000` | zero |
| `0x00a4` | `0x84010be6` | firmware address |
| `0x00a8` | `0x00000801` | scalar or packed data |
| `0x00ac` | `0x1e9000f0` | NPU SRAM/peripheral address |
| `0x00b0` | `0x1e9000f1` | NPU SRAM/peripheral address |
| `0x00b4` | `0x1e9000f0` | NPU SRAM/peripheral address |
| `0x00b8` | `0x1e9000f0` | NPU SRAM/peripheral address |
| `0x00bc` | `0x1e9000f0` | NPU SRAM/peripheral address |
| `0x00c0` | `0x1e9000f0` | NPU SRAM/peripheral address |
| `0x00c4` | `0x1e9000d0` | NPU SRAM/peripheral address |
| `0x00c8` | `0x00000000` | zero |
| `0x00cc` | `0x00000000` | zero |
| `0x00d0` | `0x00000000` | zero |
| `0x00d4` | `0x00000000` | zero |
| `0x00d8` | `0x00000000` | zero |
| `0x00dc` | `0x00000000` | zero |
| `0x00e0` | `0x00000000` | zero |
| `0x00e4` | `0x00000000` | zero |
| `0x00e8` | `0x00000000` | zero |
| `0x00ec` | `0x00000000` | zero |
| `0x00f0` | `0x00000000` | zero |
| `0x00f4` | `0x01010802` | scalar or packed data |
| `0x00f8` | `0x1e90013c` | NPU SRAM/peripheral address |
| `0x00fc` | `0x1e90013d` | NPU SRAM/peripheral address |
| `0x0100` | `0x1e90013c` | NPU SRAM/peripheral address |
| `0x0104` | `0x1e90013c` | NPU SRAM/peripheral address |
| `0x0108` | `0x1e90013c` | NPU SRAM/peripheral address |
| `0x010c` | `0x1e90013c` | NPU SRAM/peripheral address |
| `0x0110` | `0x1e90011c` | NPU SRAM/peripheral address |
| `0x0114` | `0x00000000` | zero |
| `0x0118` | `0x00000000` | zero |
| `0x011c` | `0x00000000` | zero |
| `0x0120` | `0x00000000` | zero |
| `0x0124` | `0x00000000` | zero |
| `0x0128` | `0x00000000` | zero |
| `0x012c` | `0x00000000` | zero |
| `0x0130` | `0x00000000` | zero |
| `0x0134` | `0x00000000` | zero |
| `0x0138` | `0x00000000` | zero |
| `0x013c` | `0x00000000` | zero |
| `0x0140` | `0x02020802` | scalar or packed data |
| `0x0144` | `0x1e901ddc` | NPU SRAM/peripheral address |
| `0x0148` | `0x1e901e2c` | NPU SRAM/peripheral address |
| `0x014c` | `0x1e901ddc` | NPU SRAM/peripheral address |
| `0x0150` | `0x1e901ddc` | NPU SRAM/peripheral address |
| `0x0154` | `0x1e901ddc` | NPU SRAM/peripheral address |
| `0x0158` | `0x1e900188` | NPU SRAM/peripheral address |
| `0x015c` | `0x1e900168` | NPU SRAM/peripheral address |
| `0x0160` | `0x00000000` | zero |
| `0x0164` | `0x00000000` | zero |
| `0x0168` | `0x00000000` | zero |
| `0x016c` | `0x00000000` | zero |
| `0x0170` | `0x00000000` | zero |
| `0x0174` | `0x00000000` | zero |
| `0x0178` | `0x00000000` | zero |
| `0x017c` | `0x00000000` | zero |
| `0x0180` | `0x00000000` | zero |
| `0x0184` | `0x00000000` | zero |
| `0x0188` | `0x00000000` | zero |
| `0x018c` | `0x1e9000a8` | NPU SRAM/peripheral address |
| `0x0190` | `0x1e9000f4` | NPU SRAM/peripheral address |
| `0x0194` | `0x1e900140` | NPU SRAM/peripheral address |
| `0x0198` | `0x00000000` | zero |
| `0x019c` | `0x00000000` | zero |
| `0x01a0` | `0x00000000` | zero |
| `0x01a4` | `0x00000000` | zero |
| `0x01a8` | `0x00000000` | zero |
| `0x01ac` | `0x00000000` | zero |
| `0x01b0` | `0x00000000` | zero |
| `0x01b4` | `0x00000000` | zero |
| `0x01b8` | `0x00000000` | zero |
| `0x01bc` | `0x00000000` | zero |
| `0x01c0` | `0x00000000` | zero |
| `0x01c4` | `0x00000000` | zero |
| `0x01c8` | `0x00000000` | zero |
| `0x01cc` | `0x00000000` | zero |
| `0x01d0` | `0x00000000` | zero |
| `0x01d4` | `0x00000000` | zero |
| `0x01d8` | `0x00000000` | zero |
| `0x01dc` | `0x1e9000a8` | NPU SRAM/peripheral address |
| `0x01e0` | `0x1e9000f4` | NPU SRAM/peripheral address |
| `0x01e4` | `0x1e900140` | NPU SRAM/peripheral address |
| `0x01e8` | `0x342e3656` | NPU SRAM/peripheral address |
| `0x01ec` | `0x30303a30` | NPU SRAM/peripheral address |
| `0x01f0` | `0x2d203930` | NPU SRAM/peripheral address |
| `0x01f4` | `0x6f43202d` | scalar or packed data |
| `0x01f8` | `0x69727970` | scalar or packed data |
| `0x01fc` | `0x20746867` | NPU SRAM/peripheral address |
| `0x0200` | `0x20296328` | NPU SRAM/peripheral address |
| `0x0204` | `0x32393931` | NPU SRAM/peripheral address |
| `0x0208` | `0x3130322d` | NPU SRAM/peripheral address |
| `0x020c` | `0x79622034` | scalar or packed data |
| `0x0210` | `0x4a2e5020` | scalar or packed data |
| `0x0214` | `0x6c50202e` | scalar or packed data |
| `0x0218` | `0x65677561` | scalar or packed data |
| `0x021c` | `0x6c202c72` | scalar or packed data |
| `0x0220` | `0x6e656369` | scalar or packed data |
| `0x0224` | `0x20646573` | NPU SRAM/peripheral address |
| `0x0228` | `0x44207962` | scalar or packed data |
| `0x022c` | `0x756b6e69` | scalar or packed data |
| `0x0230` | `0x7261776d` | scalar or packed data |
| `0x0234` | `0x4c202c65` | scalar or packed data |
| `0x0238` | `0x202e6474` | NPU SRAM/peripheral address |
| `0x023c` | `0x204c4c41` | NPU SRAM/peripheral address |
| `0x0240` | `0x48474952` | scalar or packed data |
| `0x0244` | `0x52205354` | scalar or packed data |
| `0x0248` | `0x52455345` | scalar or packed data |
| `0x024c` | `0x2e444556` | NPU SRAM/peripheral address |
| `0x0250` | `0x00000000` | zero |
| `0x0254` | `0x1e901e34` | NPU SRAM/peripheral address |
| `0x0258` | `0x00000050` | scalar or packed data |
| `0x025c` | `0x00000050` | scalar or packed data |
| `0x0260` | `0x00000200` | scalar or packed data |
| `0x0264` | `0x00000000` | zero |
| `0x0268` | `0x00000000` | zero |
| `0x026c` | `0x00000000` | zero |
| `0x0270` | `0x00000000` | zero |
| `0x0274` | `0x90000000` | scalar or packed data |
| `0x0278` | `0x99999999` | scalar or packed data |
| `0x027c` | `0x3ffb9999` | NPU SRAM/peripheral address |
| `0x0280` | `0x00000000` | zero |
| `0x0284` | `0x32000000` | NPU SRAM/peripheral address |
| `0x0288` | `0x33333333` | NPU SRAM/peripheral address |
| `0x028c` | `0x3fc63333` | NPU SRAM/peripheral address |
| `0x0290` | `0x00000000` | zero |
| `0x0294` | `0x32000000` | NPU SRAM/peripheral address |
| `0x0298` | `0x33333333` | NPU SRAM/peripheral address |
| `0x029c` | `0x3f8e3333` | NPU SRAM/peripheral address |
| `0x02a0` | `0x84010de6` | firmware address |
| `0x02a4` | `0x84010de6` | firmware address |
| `0x02a8` | `0x84010de6` | firmware address |
| `0x02ac` | `0x84010de6` | firmware address |
| `0x02b0` | `0x84010de6` | firmware address |
| `0x02b4` | `0x84010de6` | firmware address |
| `0x02b8` | `0x84010de6` | firmware address |
| `0x02bc` | `0xffffffff` | scalar or packed data |
| `0x02c0` | `0xffffffff` | scalar or packed data |
| `0x02c4` | `0xffffffff` | scalar or packed data |
| `0x02c8` | `0x0000ffff` | scalar or packed data |
| `0x02cc` | `0x8400fadf` | firmware address |
| `0x02d0` | `0x84010de6` | firmware address |
| `0x02d4` | `0x84010de6` | firmware address |
| `0x02d8` | `0x84010de6` | firmware address |
| `0x02dc` | `0x84010de6` | firmware address |
| `0x02e0` | `0x8400fe00` | firmware address |
| `0x02e4` | `0x8401017b` | firmware address |
| `0x02e8` | `0x84010de6` | firmware address |
| `0x02ec` | `0x84010de6` | firmware address |
| `0x02f0` | `0x0000003d` | scalar or packed data |
| `0x02f4` | `0x00000006` | scalar or packed data |
