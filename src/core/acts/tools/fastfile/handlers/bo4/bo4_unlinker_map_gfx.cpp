#include <includes.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/utils/data_utils.hpp>
#include <tools/fastfile/handlers/bo4/bo4_xpak_reader.hpp>

namespace {
    using namespace fastfile::handlers::bo4;
    using namespace games::bo4::pool;

    struct GfxBrushModel;
    struct GroupLodModel;
    struct GfxWorldLighting;
    struct StreamerWorld;

    struct GfxVisArray {
        uint32_t visDataCount;
        byte* visData;
    };

    struct GfxVolumeDecal {
        uint64_t id;
        byte hidden;
        vec3_t localToWld[4];
        vec3_t wldToLocal[4];
        vec3_t halfExtents;
        vec3_t edgeFeather;
        vec4_t revealTextureUV;
        vec4_t uvBaseAndScale;
        vec4_t unka8;
        uintptr_t material;
        uintptr_t forwardMaterial;
        uint32_t unkc8;
        uint32_t targetName;
        uint64_t unkd0;
    };

    struct GfxStaticModelDrawInst {
        XModel* model;
        uint64_t unk8;
        void* unk10;
        uintptr_t unk18;
        int32_t unk20;
        ScrString_t name;
        uint32_t unk28;
        uint16_t unk2c;
        uint16_t unk2e;
        uint32_t unk30;
        int32_t unk34;
    };

    struct GfxWorldDpvsStatic {
        uint32_t smodelUpdateFrame;
        uint32_t smodelCount;
        uint64_t unk08;
        uint64_t unk10;
        uint64_t unk18;
        uint64_t unk20;
        uint64_t unk28;
        uint64_t unk30;
        uint64_t unk38;
        GfxVisArray smodelVisData[4];
        GfxVisArray surfaceVisData[4];
        GfxVisArray volDecalVisData[4];
        uint64_t unk100;
        uint64_t unk108;
        uint64_t unk110;
        uint64_t unk118;
        uint64_t unk120;
        uint64_t unk128;
        uint64_t unk130;
        uint64_t unk138;
        uint64_t unk140;
        uint64_t unk148;
        uint64_t unk150;
        uint64_t unk158;
        uint64_t unk160;
        uint64_t unk168;
        uint64_t unk170;
        uint64_t unk178;
        uint64_t unk180;
        uint64_t unk188;
        uint64_t unk190;
        uint64_t unk198;
        uint64_t unk1a0;
        uint64_t unk1a8;
        uint64_t unk1b0;
        uint64_t unk1b8;
        uint64_t unk1c0;
        uint64_t unk1c8;
        uint64_t unk1d0;
        uint64_t unk1d8;
        uint64_t unk1e0;
        uint64_t unk1e8;
        uint64_t unk1f0;
        uint64_t unk1f8;
        uint64_t unk200;
        uint64_t unk208;
        uint64_t unk210;
        uint64_t unk218;
        uint64_t unk220;
        uint64_t unk228;
        uint64_t unk230;
        uint64_t unk238;
        uint64_t unk240;
        GfxStaticModelDrawInst* smodelDrawInsts;
        uint64_t unk250;
        uint64_t unk258;
        uint64_t unk260;
        uint64_t unk268;
        uint64_t unk270;
        uint64_t unk278;
        uint64_t unk280;
        uint64_t unk288;
        uint64_t unk290;
        uint64_t unk298;
        uint64_t unk2a0;
        uint64_t unk2a8;
        uint64_t unk2b0;
        uint64_t unk2b8;
        uint64_t unk2c0;
        uint64_t unk2c8;
        uint64_t groupLodModelsCount;
        GroupLodModel** groupLodModels;
    };

    struct GfxWorldDraw {
        uint32_t blubIESprofileCNT;
        GfxImage* blubIESprofileIMG;
        int32_t cookieCount;
        GfxImage* cookieArray;
        uint32_t unk20;
        uint32_t unk24;
        uint64_t unk28;
        uint64_t unk30;
        uint64_t unk38;
        uint64_t unk40;
        uint64_t unk48;
        uint64_t unk50;
        uint64_t unk58;
        uint64_t unk60;
        uint64_t unk68;
        uint64_t unk70;
        uint32_t indexCount;
        uint32_t unk7c;
        uint16_t* indices;
        uint64_t unk88;
        uint64_t unk90;
        uint64_t unk98;
        uint64_t unka0;
    };

    struct GfxWorld {
        XHash name;
        const char* baseName;
        int32_t surfaceCount;
        uint64_t unk20;
        uint64_t unk28;
        uint64_t unk30;
        uint64_t unk38;
        uint64_t unk40;
        uint64_t unk48;
        uint64_t unk50;
        uint64_t unk58;
        uint64_t unk60;
        uint64_t unk68;
        uint64_t unk70;
        uint64_t unk78;
        uint64_t unk80;
        uint64_t unk88;
        uint64_t unk90;
        uint64_t unk98;
        uint64_t unka0;
        uint64_t unka8;
        uint64_t unkb0;
        vec3_t* unkb8;
        uint64_t unkc0;
        uint64_t unkc8;
        uint64_t unkd0;
        GfxWorldDraw draw;
        int32_t modelCount;
        uint32_t unk184;
        GfxBrushModel* models;
        vec3_t mins;
        vec3_t maxs;
        uint32_t checksum;
        int32_t materialMemoryCount;
        uint64_t unk1b0;
        GfxWorldDpvsStatic dpvs;
        uint32_t unk498;
        uint32_t unk49c;
        uint64_t unk4a0;
        uint64_t unk4a8;
        uint64_t unk4b0;
        uint64_t unk4b8;
        uint64_t unk4c0;
        uint64_t unk4c8;
        uint64_t unk4d0;
        uint64_t unk4d8;
        uint64_t unk4e0;
        uint64_t unk4e8;
        uint64_t unk4f0;
        uint64_t unk4f8;
        uint64_t unk500;
        uint64_t unk508;
        uint64_t unk510;
        uint64_t unk518;
        uint64_t unk520;
        uint64_t unk528;
        uint64_t unk530;
        uint64_t unk538;
        Material* lutMaterial;
        Material* lutMaterialALT;
        GfxImage* unk550[4];
        uint64_t unk570;
        uint64_t unk578;
        uint64_t unk580;
        uint64_t unk588;
        uint32_t volumeDecalCount;
        GfxVolumeDecal* volumeDecals;
        uint64_t* volumeDecalUnk5a0;
        GfxImage* volumeDecalRevealTexture;
        uint64_t unk5b0;
        uint64_t unk5b8;
        uint64_t unk5c0;
        int32_t unk5c8;
        uint32_t unk5cc;
        uint64_t unk5d0;
        uint64_t unk5d8;
        uint64_t unk5e0;
        uint64_t unk5e8;
        uint64_t unk5f0;
        uint64_t unk5f8;
        uint64_t unk600;
        uint64_t unk608;
        uint64_t unk610;
        uint64_t unk618;
        uint64_t unk620;
        uint64_t unk628;
        uint64_t unk630;
        uint64_t unk638;
        uint64_t unk640;
        uint64_t unk648;
        uint64_t unk650;
        uint64_t unk658;
        uint64_t unk660;
        uint64_t unk668;
        uint64_t unk670;
        uint64_t unk678;
        uint64_t unk680;
        uint64_t unk688;
        uint64_t unk690;
        uint64_t unk698;
        uint64_t unk6a0;
        uint64_t unk6a8;
        uint64_t unk6b0;
        uint64_t unk6b8;
        uint64_t unk6c0;
        uint64_t unk6c8;
        uint64_t unk6d0;
        uint64_t unk6d8;
        uint64_t unk6e0;
        uint64_t unk6e8;
        uint64_t unk6f0;
        uint64_t unk6f8;
        uint64_t unk700;
        uint64_t unk708;
        uint64_t unk710;
        uint64_t unk718;
        uint64_t unk720;
        uint64_t unk728;
        uint64_t unk730;
        uint64_t unk738;
        uint64_t unk740;
        uint64_t unk748;
        uint64_t unk750;
        uint64_t unk758;
        uint64_t unk760;
        uint64_t unk768;
        uint64_t unk770;
        uint64_t unk778;
        uint64_t unk780;
        uint64_t unk788;
        uint64_t unk790;
        uint64_t unk798;
        uint64_t unk7a0;
        uint64_t unk7a8;
        uint64_t unk7b0;
        uint64_t unk7b8;
        uint64_t unk7c0;
        uint64_t unk7c8;
        uint64_t unk7d0;
        uint64_t unk7d8;
        uint64_t unk7e0;
        uint64_t unk7e8;
        uint64_t unk7f0;
        uint64_t unk7f8;
        uint64_t unk800;
        uint64_t unk808;
        uint64_t unk810;
        uint64_t unk818;
        uint64_t unk820;
        uint64_t unk828;
        uint64_t unk830;
        uint64_t unk838;
        uint64_t unk840;
        uint64_t unk848;
        uint64_t unk850;
        uint64_t unk858;
        uint64_t unk860;
        uint64_t unk868;
        uint64_t unk870;
        uint64_t unk878;
        uint64_t unk880;
        uint64_t unk888;
        uint64_t unk890;
        uint64_t unk898;
        uint64_t unk8a0;
        uint64_t unk8a8;
        uint64_t unk8b0;
        uint64_t unk8b8;
        uint64_t unk8c0;
        uint64_t unk8c8;
        uint64_t unk8d0;
        uint64_t unk8d8;
        uint64_t unk8e0;
        uint64_t unk8e8;
        uint64_t unk8f0;
        uint64_t unk8f8;
        uint64_t unk900;
        uint64_t unk908;
        uint64_t unk910;
        uint64_t unk918;
        uint64_t unk920;
        uint64_t unk928;
        uint64_t unk930;
        uint64_t unk938;
        uint64_t unk940;
        uint64_t unk948;
        uint64_t unk950;
        uint64_t unk958;
        uint64_t unk960;
        uint64_t unk968;
        uint64_t unk970;
        uint64_t unk978;
        uint64_t unk980;
        uint64_t unk988;
        uint64_t unk990;
        uint64_t unk998;
        uint64_t unk9a0;
        uint64_t unk9a8;
        uint64_t unk9b0;
        uint64_t unk9b8;
        uint64_t unk9c0;
        uint64_t unk9c8;
        uint64_t unk9d0;
        uint64_t unk9d8;
        uint64_t unk9e0;
        uint64_t unk9e8;
        uint64_t unk9f0;
        uint64_t unk9f8;
        uint64_t unka00;
        uint64_t unka08;
        uint64_t unka10;
        uint64_t unka18;
        uint64_t unka20;
        uint64_t unka28;
        uint64_t unka30;
        uint64_t unka38;
        uint64_t unka40;
        uint64_t unka48;
        uint64_t unka50;
        uint64_t unka58;
        uint64_t unka60;
        uint64_t unka68;
        uint64_t unka70;
        uint64_t unka78;
        uint64_t unka80;
        uint64_t unka88;
        uint64_t unka90;
        uint64_t unka98;
        uint64_t unkaa0;
        uint64_t unkaa8;
        uint64_t unkab0;
        uint64_t unkab8;
        uint64_t unkac0;
        uint64_t unkac8;
        uint64_t unkad0;
        uint64_t unkad8;
        uint64_t unkae0;
        uint64_t unkae8;
        uint64_t unkaf0;
        uint64_t unkaf8;
        uint64_t unkb00;
        uint64_t unkb08;
        uint64_t unkb10;
        uint64_t unkb18;
        uint64_t unkb20;
        uint64_t unkb28;
        uint64_t unkb30;
        uint64_t unkb38;
        uint64_t unkb40;
        uint64_t unkb48;
        uint64_t unkb50;
        uint64_t unkb58;
        uint64_t unkb60;
        uint64_t unkb68;
        uint64_t unkb70;
        uint64_t unkb78;
        uint64_t unkb80;
        uint64_t unkb88;
        uint64_t unkb90;
        uint64_t unkb98;
        uint64_t unkba0;
        uint64_t unkba8;
        uint64_t unkbb0;
        uint64_t unkbb8;
        uint64_t unkbc0;
        uint64_t unkbc8;
        uint64_t unkbd0;
        uint64_t unkbd8;
        uint64_t unkbe0;
        uint64_t unkbe8;
        uint64_t unkbf0;
        uint64_t unkbf8;
        uint64_t unkc00;
        uint64_t unkc08;
        uint64_t unkc10;
        uint64_t unkc18;
        uint64_t unkc20;
        uint64_t unkc28;
        uint64_t unkc30;
        uint64_t unkc38;
        uint64_t unkc40;
        uint64_t unkc48;
        uint64_t unkc50;
        uint64_t unkc58;
        uint64_t unkc60;
        uint64_t unkc68;
        uint64_t unkc70;
        uint64_t unkc78;
        uint64_t unkc80;
        uint64_t unkc88;
        uint64_t unkc90;
        uint64_t unkc98;
        uint64_t unkca0;
        uint64_t unkca8;
        uint64_t unkcb0;
        uint64_t unkcb8;
        uint64_t unkcc0;
        uint64_t unkcc8;
        uint64_t unkcd0;
        uint64_t unkcd8;
        uint64_t unkce0;
        uint64_t unkce8;
        uint64_t unkcf0;
        uint64_t unkcf8;
        uint64_t unkd00;
        uint64_t unkd08;
        uint64_t unkd10;
        uint64_t unkd18;
        uint64_t unkd20;
        uint64_t unkd28;
        uint64_t unkd30;
        uint64_t unkd38;
        uint64_t unkd40;
        uint64_t unkd48;
        uint64_t unkd50;
        uint64_t unkd58;
        uint64_t unkd60;
        uint64_t unkd68;
        uint64_t unkd70;
        uint64_t unkd78;
        uint64_t unkd80;
        uint64_t unkd88;
        uint64_t unkd90;
        uint64_t unkd98;
        uint64_t unkda0;
        uint64_t unkda8;
        uint64_t unkdb0;
        uint64_t unkdb8;
        uint64_t unkdc0;
        uint64_t unkdc8;
        uint64_t unkdd0;
        uint64_t unkdd8;
        uint64_t unkde0;
        uint64_t unkde8;
        uint64_t unkdf0;
        uint64_t unkdf8;
        uint64_t unke00;
        uint64_t unke08;
        uint64_t unke10;
        uint64_t unke18;
        uint64_t unke20;
        uint64_t unke28;
        uint64_t unke30;
        uint64_t unke38;
        uint64_t unke40;
        uint64_t unke48;
        uint64_t unke50;
        uint64_t unke58;
        uint64_t unke60;
        uint64_t unke68;
        uint64_t unke70;
        uint64_t unke78;
        uint64_t unke80;
        uint64_t unke88;
        uint64_t unke90;
        uint64_t unke98;
        uint64_t unkea0;
        uint64_t unkea8;
        uint64_t unkeb0;
        uint64_t unkeb8;
        uint64_t unkec0;
        uint64_t unkec8;
        uint64_t unked0;
        uint64_t unked8;
        uint64_t unkee0;
        uint64_t unkee8;
        uint64_t unkef0;
        uint64_t unkef8;
        uint64_t unkf00;
        uint64_t unkf08;
        uint64_t unkf10;
        uint64_t unkf18;
        uint64_t unkf20;
        uint64_t unkf28;
        uint64_t unkf30;
        uint64_t unkf38;
        uint64_t unkf40;
        uint64_t unkf48;
        uint64_t unkf50;
        uint64_t unkf58;
        uint64_t unkf60;
        uint64_t unkf68;
        uint64_t unkf70;
        uint64_t unkf78;
        uint64_t unkf80;
        uint64_t unkf88;
        uint64_t unkf90;
        uint64_t unkf98;
        uint64_t unkfa0;
        uint64_t unkfa8;
        uint64_t unkfb0;
        uint64_t unkfb8;
        uint64_t unkfc0;
        uint64_t unkfc8;
        uint64_t unkfd0;
        uint64_t unkfd8;
        uint64_t unkfe0;
        uint64_t unkfe8;
        uint64_t unkff0;
        uint64_t unkff8;
        uint64_t unk1000;
        uint64_t unk1008;
        uint64_t unk1010;
        uint64_t unk1018;
        uint64_t unk1020;
        uint64_t unk1028;
        uint64_t unk1030;
        uint64_t unk1038;
        uint64_t unk1040;
        uint64_t unk1048;
        uint64_t unk1050;
        uint64_t unk1058;
        uint64_t unk1060;
        uint64_t unk1068;
        uint64_t unk1070;
        uint64_t unk1078;
        uint64_t unk1080;
        uint64_t unk1088;
        uint64_t unk1090;
        uint64_t unk1098;
        uint64_t unk10a0;
        uint64_t unk10a8;
        uint64_t unk10b0;
        uint64_t unk10b8;
        uint64_t unk10c0;
        uint64_t unk10c8;
        uint64_t unk10d0;
        uint64_t unk10d8;
        uint64_t unk10e0;
        uint64_t unk10e8;
        uint64_t unk10f0;
        uint64_t unk10f8;
        uint64_t unk1100;
        uint64_t unk1108;
        uint64_t unk1110;
        uint64_t unk1118;
        uint64_t unk1120;
        uint64_t unk1128;
        uint64_t unk1130;
        uint64_t unk1138;
        uint64_t unk1140;
        uint64_t unk1148;
        uint64_t unk1150;
        uint64_t unk1158;
        uint64_t unk1160;
        uint64_t unk1168;
        uint64_t unk1170;
        uint64_t unk1178;
        uint64_t unk1180;
        uint64_t unk1188;
        uint64_t unk1190;
        uint64_t unk1198;
        uint64_t unk11a0;
        uint64_t unk11a8;
        uint64_t unk11b0;
        uint64_t unk11b8;
        uint64_t unk11c0;
        uint64_t unk11c8;
        uint64_t unk11d0;
        uint64_t unk11d8;
        uint64_t unk11e0;
        uint64_t unk11e8;
        uint64_t unk11f0;
        uint64_t unk11f8;
        uint64_t unk1200;
        uint64_t unk1208;
        uint64_t unk1210;
        uint64_t unk1218;
        uint64_t unk1220;
        uint64_t unk1228;
        uint64_t unk1230;
        uint64_t unk1238;
        uint64_t unk1240;
        uint64_t unk1248;
        uint64_t unk1250;
        uint64_t unk1258;
        uint64_t unk1260;
        uint64_t unk1268;
        uint64_t unk1270;
        uint64_t unk1278;
        uint64_t unk1280;
        uint64_t unk1288;
        uint64_t unk1290;
        uint64_t unk1298;
        uint64_t unk12a0;
        uint64_t unk12a8;
        uint64_t unk12b0;
        uint64_t unk12b8;
        uint64_t unk12c0;
        uint64_t unk12c8;
        uint64_t unk12d0;
        uint64_t unk12d8;
        uint64_t unk12e0;
        uint64_t unk12e8;
        uint64_t unk12f0;
        uint64_t unk12f8;
        uint64_t unk1300;
        uint64_t unk1308;
        uint64_t unk1310;
        uint64_t unk1318;
        uint64_t unk1320;
        uint64_t unk1328;
        uint64_t unk1330;
        uint64_t unk1338;
        uint64_t unk1340;
        uint64_t unk1348;
        uint64_t unk1350;
        uint64_t unk1358;
        uint64_t unk1360;
        uint64_t unk1368;
        uint64_t unk1370;
        uint64_t unk1378;
        uint64_t unk1380;
        uint64_t unk1388;
        uint64_t unk1390;
        uint64_t unk1398;
        uint64_t unk13a0;
        uint64_t unk13a8;
        uint64_t unk13b0;
        uint64_t unk13b8;
        uint64_t unk13c0;
        uint64_t unk13c8;
        uint64_t unk13d0;
        uint64_t unk13d8;
        uint64_t unk13e0;
        uint64_t unk13e8;
        uint64_t unk13f0;
        uint64_t unk13f8;
        uint64_t unk1400;
        uint64_t unk1408;
        uint64_t unk1410;
        uint64_t unk1418;
        uint64_t unk1420;
        uint64_t unk1428;
        uint64_t unk1430;
        uint64_t unk1438;
        uint64_t unk1440;
        uint64_t unk1448;
        uint64_t unk1450;
        uint64_t unk1458;
        uint64_t unk1460;
        uint64_t unk1468;
        uint64_t unk1470;
        uint64_t unk1478;
        uint64_t unk1480;
        uint64_t unk1488;
        uint64_t unk1490;
        uint64_t unk1498;
        uint64_t unk14a0;
        uint64_t unk14a8;
        uint64_t unk14b0;
        uint64_t unk14b8;
        uint64_t unk14c0;
        uint64_t unk14c8;
        uint64_t unk14d0;
        uint64_t unk14d8;
        uint64_t unk14e0;
        uint64_t unk14e8;
        uint64_t unk14f0;
        uint64_t unk14f8;
        uint64_t unk1500;
        uint64_t unk1508;
        uint64_t unk1510;
        uint64_t unk1518;
        uint64_t unk1520;
        uint64_t unk1528;
        uint64_t unk1530;
        uint64_t unk1538;
        uint64_t unk1540;
        uint64_t unk1548;
        uint64_t unk1550;
        uint64_t unk1558;
        uint64_t unk1560;
        uint64_t unk1568;
        uint64_t unk1570;
        uint64_t unk1578;
        uint64_t unk1580;
        uint64_t unk1588;
        uint64_t unk1590;
        uint64_t unk1598;
        uint64_t unk15a0;
        uint64_t unk15a8;
        uint64_t unk15b0;
        uint64_t unk15b8;
        uint64_t unk15c0;
        uint64_t unk15c8;
        uint64_t unk15d0;
        uint64_t unk15d8;
        uint64_t unk15e0;
        uint64_t unk15e8;
        uint64_t unk15f0;
        uint64_t unk15f8;
        uint64_t unk1600;
        uint64_t unk1608;
        uint64_t unk1610;
        uint64_t unk1618;
        uint64_t unk1620;
        uint64_t unk1628;
        uint64_t unk1630;
        uint64_t unk1638;
        uint64_t unk1640;
        uint64_t unk1648;
        uint64_t unk1650;
        uint64_t unk1658;
        uint64_t unk1660;
        uint64_t unk1668;
        uint64_t unk1670;
        uint64_t unk1678;
        uint64_t unk1680;
        uint64_t unk1688;
        uint64_t unk1690;
        uint64_t unk1698;
        uint64_t unk16a0;
        uint64_t unk16a8;
        uint64_t unk16b0;
        uint32_t unk16b8;
        uint32_t unk16bc;
        uint64_t unk16c0;
        uint32_t unk16c8[4];
        uint32_t unk16d8[32];
        uint32_t unk1a48_count;
        uint32_t unk175c;
        uint64_t unk1760;
        uint64_t unk1768;
        uint64_t unk1770;
        uint64_t unk1778;
        uint64_t unk1780;
        uint32_t unk1a38_count;
        uint32_t unk178c;
        uint64_t unk1790;
        uint64_t unk1798;
        uint64_t unk17a0;
        uint64_t unk17a8;
        uint64_t unk17b0;
        uint64_t unk17b8;
        uint64_t unk17c0;
        uint64_t unk17c8;
        uint64_t unk17d0;
        uint32_t unk1a40_count;
        uint32_t unk17dc;
        uint64_t unk17e0;
        uint64_t unk17e8;
        uint64_t unk17f0;
        GfxWorldLighting* lighting;
        StreamerWorld* streamerworld;
        uint64_t unk1808;
        uint64_t unk1810;
        uint64_t unk1818;
        uint64_t unk1820;
        uint64_t unk1828;
        uint64_t unk1830;
        uint64_t unk1838;
        uint64_t unk1840;
        uint64_t unk1848;
        uint64_t unk1850;
        uint64_t unk1858;
        uint64_t unk1860;
        uint64_t unk1868;
        uint64_t unk1870;
        uint64_t unk1878;
        uint64_t unk1880;
        uint64_t unk1888;
        uint64_t unk1890;
        uint32_t unk1898[4];
        uint32_t unk18a8[32];
        uint64_t unk1928;
        uint64_t unk1930;
        uint64_t unk1938;
        uint64_t unk1940;
        uint64_t unk1948;
        uint64_t unk1950;
        uint32_t unk1a50_count;
        uint32_t unk195c;
        uint64_t unk1960;
        uint32_t unk1968[4];
        uint32_t unk1978[32];
        uint32_t unk19f8;
        uint32_t unk19fc;
        uint64_t unk1a00;
        uint64_t unk1a08;
        uint64_t unk1a10;
        uint64_t unk1a18;
        uint64_t unk1a20;
        uint32_t unk1a58_count;
        uint32_t unk1a2c;
        uint64_t unk1a30;
        byte* unk1a38;
        byte* unk1a40;
        byte* unk1a48;
        byte* unk1a50;
        byte* unk1a58;
        GfxImage* unk1a60[10];
    };

    static_assert(sizeof(GfxWorld) == 0x1ab0);

    /*
     * The arrays of the retail loader (sub_142E1E830) as offsets in the 0x1AB0 header: the pointer, its count (4 or 8
     * bytes) and the size of an element. The raw dump writes the header and each of them, the templates of the linker.
     */
    struct GfxWorldRawArray {
        uint32_t ptr;
        uint32_t count;
        uint32_t countSize;
        uint32_t elemSize;
    };
    constexpr GfxWorldRawArray gfxWorldRawArrays[]{
        { 0xC0, 0xB8, 8, 0xC }, { 0xD0, 0xC8, 8, 0x10 }, { 0x188, 0x180, 4, 0x50 }, { 0x578, 0x570, 8, 0x18 },
        { 0x588, 0x580, 4, 0x54 }, { 0x598, 0x590, 4, 0xD8 }, { 0x5F0, 0x5E8, 4, 0xC0 }, { 0x600, 0x5F8, 4, 0xC0 },
        { 0x1620, 0x1618, 4, 0x40 }, { 0x1630, 0x1628, 4, 0xB0 }, { 0x1640, 0x1638, 4, 0xB0 }, { 0x1660, 0x1658, 4, 0x40 },
        { 0x1670, 0x1668, 4, 0xB0 }, { 0x1680, 0x1678, 4, 0xB0 }, { 0x1690, 0x1688, 4, 0xC4 }, { 0x16A0, 0x1698, 4, 0xC4 },
        { 0x16B0, 0x16A8, 4, 0xB4 }, { 0x16C0, 0x16B8, 4, 0xB4 }, { 0x1760, 0x1758, 4, 0x200 }, { 0x1770, 0x1768, 4, 0xB4 },
        { 0x1780, 0x1778, 4, 0xB4 }, { 0x1790, 0x1788, 4, 0x54 }, { 0x17A0, 0x1798, 4, 0xB4 }, { 0x17B0, 0x17A8, 4, 0xB4 },
        { 0x17C0, 0x17B8, 4, 0xB4 }, { 0x17D0, 0x17C8, 4, 0xB4 }, { 0x17E0, 0x17D8, 4, 0x90 }, { 0x17F0, 0x17E8, 4, 0xE0 },
        { 0x1930, 0x1928, 4, 0x48 }, { 0x1940, 0x1938, 4, 0xB8 }, { 0x1950, 0x1948, 4, 0xB8 }, { 0x1960, 0x1958, 4, 0xFD0 },
        { 0x1A00, 0x19F8, 4, 0x48 }, { 0x1A10, 0x1A08, 4, 0xB8 }, { 0x1A20, 0x1A18, 4, 0xB8 }, { 0x1A30, 0x1A28, 4, 0x2B0 },
        { 0x1A38, 0x1788, 4, 0x40 }, { 0x1A40, 0x17D8, 4, 0x40 }, { 0x1A48, 0x1758, 4, 0x40 }, { 0x1A50, 0x1958, 4, 0x40 },
        { 0x1A58, 0x1A28, 4, 0x40 },
        // in draw (0xD8): the vertex data (0xC and 0x14 by vertex), the indices
        { 0x100, 0xF8, 4, 0xC }, { 0x128, 0xF8, 4, 0x14 }, { 0x158, 0x150, 4, 0x2 },
    };

    void DumpGfxWorldRaw(fastfile::FastFileOption& opt, const std::filesystem::path& outDir, const GfxWorld& asset) {
        std::filesystem::path rawDir{ outDir / "gfx_map_raw" };
        std::filesystem::create_directories(rawDir);
        const byte* header{ (const byte*)&asset };
        utils::WriteFile(rawDir / "header.bin", header, sizeof(asset));
        // the surfaces (dpvs 0x240 = 0x3F8, 0x60 each, count 0x18, material at 0x48) and their materials' names
        int32_t surfaceCount{ *(int32_t*)(header + 0x18) };
        const byte* surfaces{ *(const byte* const*)(header + 0x3F8) };
        if (surfaces && surfaceCount > 0) {
            utils::WriteFile(rawDir / "surfaces.bin", surfaces, 0x60 * (size_t)surfaceCount);
            BO4JsonWriter sj{};
            sj.BeginArray();
            for (int32_t i = 0; i < surfaceCount; i++) {
                void* mat{ *(void* const*)(surfaces + 0x60 * i + 0x48) };
                sj.BeginObject();
                sj.WriteFieldValueXAsset("material", ASSET_TYPE_MATERIAL, mat);
                sj.EndObject();
            }
            sj.EndArray();
            sj.WriteToFile(rawDir / "surface_materials.json");
        }
        // the dpvs block (0x1B8, 0x2E0) as it is loaded
        utils::WriteFile(rawDir / "dpvs.bin", header + 0x1B8, 0x2E0);
        /*
         * the static models (dpvs +0x04 count, sub_142E1F510): the first ones of the draw insts (+0x248, 0x38), the 0x40s
         * at +0x250, the 8s at +0x258, the bytes at +0x2A0 (count +0x298) and their models' names; the 3 cull trees (+0x188,
         * +0x1B8, +0x1E8, 0x30: data size 0, data 8, count 0x10, indices 0x18, count 0x20, bounds 0x28): their start
         */
        {
            const byte* dpvs{ header + 0x1B8 };
            const uint32_t smodelCount{ *(const uint32_t*)(dpvs + 4) };
            const size_t sample{ std::min<size_t>(smodelCount, 8) };
            auto RawAt = [&](size_t off, size_t elem, size_t count, const char* file) {
                const byte* ptr{ *(const byte* const*)(dpvs + off) };
                if (ptr && count) {
                    utils::WriteFile(rawDir / file, ptr, elem * count);
                }
            };
            RawAt(0x248, 0x38, sample, "smodel_drawinsts.bin");
            RawAt(0x250, 0x40, sample, "smodel_250.bin");
            RawAt(0x258, 0x8, sample, "smodel_258.bin");
            RawAt(0x2A0, 1, std::min<size_t>(*(const uint64_t*)(dpvs + 0x298), 0x40), "smodel_2a0.bin");
            RawAt(0x290, 1, std::min<size_t>(*(const uint64_t*)(dpvs + 0x288), 0x40), "smodel_290.bin");
            const byte* insts{ *(const byte* const*)(dpvs + 0x248) };
            if (insts) {
                BO4JsonWriter mj{};
                mj.BeginArray();
                for (size_t i = 0; i < sample; i++) {
                    mj.BeginObject();
                    mj.WriteFieldValueXAsset("model", ASSET_TYPE_XMODEL, *(void* const*)(insts + 0x38 * i));
                    mj.EndObject();
                }
                mj.EndArray();
                mj.WriteToFile(rawDir / "smodel_models.json");
            }
            for (size_t t = 0; t < 3; t++) {
                const byte* tree{ dpvs + 0x188 + 0x30 * t };
                const byte* data{ *(const byte* const*)(tree + 8) };
                const uint64_t dataSize{ *(const uint64_t*)tree };
                if (data && dataSize) {
                    utils::WriteFile(rawDir / std::format("tree{}_data.bin", t), data, std::min<size_t>(dataSize, 0x200));
                }
                const byte* indices{ *(const byte* const*)(tree + 0x18) };
                if (indices) {
                    utils::WriteFile(rawDir / std::format("tree{}_indices.bin", t), indices, 4 * std::min<size_t>(*(const uint64_t*)(tree + 0x10), 0x40));
                }
                const byte* bounds{ *(const byte* const*)(tree + 0x28) };
                if (bounds) {
                    utils::WriteFile(rawDir / std::format("tree{}_28.bin", t), bounds, std::min<size_t>(*(const uint64_t*)(tree + 0x20) * 4, 0x100));
                }
            }
        }

        // the lighting (0x1F8, sub_142E214B0) and the streamerworld (0x100, sub_142E2E760) it points to
        if (asset.lighting) {
            utils::WriteFile(rawDir / "lighting.bin", asset.lighting, 0x1F8);
            // its images: 4 groups of 3 at 0x30, 4 at 0x178
            BO4JsonWriter lj{};
            lj.BeginObject();
            lj.WriteFieldValueXAssetArray("images30", ASSET_TYPE_IMAGE, 12, (byte*)asset.lighting + 0x30, false);
            lj.WriteFieldValueXAssetArray("images178", ASSET_TYPE_IMAGE, 4, (byte*)asset.lighting + 0x178, false);
            lj.EndObject();
            lj.WriteToFile(rawDir / "lighting_images.json");

            // its 0x90/0x98 list (0x1D0 each: 0x10, then 4 blocks of 0x70 with an object at 0x60)
            uint32_t list90{ *(uint32_t*)((byte*)asset.lighting + 0x90) };
            byte* l90{ *(byte**)((byte*)asset.lighting + 0x98) };
            for (uint32_t i = 0; l90 && i < list90; i++) {
                utils::WriteFile(rawDir / std::format("lighting_90_{}.bin", i), l90 + 0x1D0 * i, 0x1D0);
                for (size_t j = 0; j < 4; j++) {
                    if (byte* obj{ *(byte**)(l90 + 0x1D0 * i + 0x10 + 0x70 * j + 0x60) }) {
                        utils::WriteFile(rawDir / std::format("lighting_90_{}_obj{}.bin", i, j), obj, 0x70);
                    }
                }
            }

            // its lighting states (0x10/0x18, 0x14C0 each), with the 4 objects at 0xF10 + 0x80 * i (0x70) and their
            // stream keys (0x48)
            uint32_t states{ *(uint32_t*)((byte*)asset.lighting + 0x10) };
            byte* stateList{ *(byte**)((byte*)asset.lighting + 0x18) };
            for (uint32_t i = 0; stateList && i < states; i++) {
                byte* state{ stateList + 0x14C0 * i };
                utils::WriteFile(rawDir / std::format("lighting_state{}.bin", i), state, 0x14C0);
                // the 4 records at 0x1240 (0x30: 3 images, the streamed data handle at 0x18, u32s 0x20/0x28)
                BO4JsonWriter rj{};
                rj.BeginObject();
                for (size_t j = 0; j < 4; j++) {
                    byte* rec{ state + 0x1240 + 0x30 * j };
                    rj.WriteFieldValueXAssetArray(utils::va("images%d", (int)j), ASSET_TYPE_IMAGE, 3, rec, false);
                    if (byte* handle{ *(byte**)(rec + 0x18) }) {
                        utils::WriteFile(rawDir / std::format("lighting_state{}_rec{}_handle.bin", i, j), handle, 0x48);
                        uint32_t size{ *(uint32_t*)(handle + 0x3C) };
                        byte* data{ *(byte**)(handle + 0x40) };
                        if (data && size && size < 0x1000000) {
                            utils::WriteFile(rawDir / std::format("lighting_state{}_rec{}_data.bin", i, j), data, size);
                        }
                    }
                    if (uint32_t* u{ *(uint32_t**)(rec + 0x28) }) {
                        utils::WriteFile(rawDir / std::format("lighting_state{}_rec{}_u32.bin", i, j), u, 4 * *(uint64_t*)(rec + 0x20));
                    }
                }
                // the 4 records at 0x10E0 (0x58: images count 0 and list 8, 0x144s at 0x10 ([0x50] + 1), 0x25Cs at 0x18
                // ([0x54]), inline 0x30 at 0x20)
                for (size_t j = 0; j < 4; j++) {
                    byte* rec{ state + 0x10E0 + 0x58 * j };
                    uint64_t count{ *(uint64_t*)rec };
                    rj.WriteFieldValueNumber(utils::va("q%dImageCount", (int)j), count);
                    rj.WriteFieldValueXAssetArray(utils::va("q%dImages", (int)j), ASSET_TYPE_IMAGE, count, *(void**)(rec + 8), false);
                    rj.WriteFieldValueNumber(utils::va("q%d50", (int)j), *(uint32_t*)(rec + 0x50));
                    rj.WriteFieldValueNumber(utils::va("q%d54", (int)j), *(uint32_t*)(rec + 0x54));
                    if (byte* d144{ *(byte**)(rec + 0x10) }) {
                        utils::WriteFile(rawDir / std::format("lighting_state{}_q{}_144.bin", i, j), d144, 0x144 * (*(uint32_t*)(rec + 0x50) + 1));
                    }
                }
                // the 4 sky entries at 0x1308 (0x28: xmodel 0, rotation 8, distance 0xC, image 0x18, the sun's KLF 0x20,
                // sub_142E1DED0), the KLF as the fx dumper writes it (the lighting linker's "lensFlare")
                for (size_t j = 0; j < 4; j++) {
                    byte* e{ state + 0x1308 + 0x28 * j };
                    rj.WriteFieldValueXAsset(utils::va("sky%dModel", (int)j), ASSET_TYPE_XMODEL, *(void**)e);
                    rj.WriteFieldValueXAsset(utils::va("sky%dImage", (int)j), ASSET_TYPE_IMAGE, *(void**)(e + 0x18));
                    if (const byte* klf{ *(const byte**)(e + 0x20) }) {
                        rj.WriteFieldValueString(utils::va("sky%dLensFlare", (int)j), std::format("#hash_{:x}", *(uint64_t*)(klf + 0x18) & 0x7FFFFFFFFFFFFFFFull));
                        BO4JsonWriter kj{};
                        WriteKlfJson(kj, klf);
                        kj.WriteToFile(rawDir / std::format("lighting_state{}_sky{}_klf.json", i, j));
                    }
                    std::string raw{};
                    for (size_t k = 0; k < 0x28; k++) {
                        raw += std::format("{:02x}", e[k]);
                    }
                    rj.WriteFieldValueString(utils::va("sky%dRaw", (int)j), raw);
                }
                rj.EndObject();
                rj.WriteToFile(rawDir / std::format("lighting_state{}_records.json", i));

                for (size_t j = 0; j < 4; j++) {
                    byte* obj{ *(byte**)(state + 0xF10 + 0x80 * j) };
                    if (!obj) {
                        continue;
                    }
                    utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}.bin", i, j), obj, 0x70);
                    if (byte* key{ *(byte**)obj }) {
                        utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_key.bin", i, j), key, 0x48);
                        // its streamed data (the xpak entry of its XPakEntryInfo at 0x10), the map's paks first
                        std::vector<byte> data{};
                        xpak::AddHint(outDir.stem().string().c_str());
                        if (*(uint64_t*)(key + 0x10) && xpak::ReadEntry(opt, *(uint64_t*)(key + 0x10), 0x40000000, data)) {
                            // the read keeps the asked size: the zeros after the data out
                            while (!data.empty() && !data.back()) {
                                data.pop_back();
                            }
                            utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_data.bin", i, j), data.data(), data.size());
                        }
                    }
                    // 0x8/0x10: u32s (count 0x1C); 0x58/0x68: 0x30 (u16s count 0x10 at 0x18, image 0x20); 0x60: 0x40 texture
                    uint32_t c{ *(uint32_t*)(obj + 0x1C) };
                    for (size_t o : { 0x8, 0x10 }) {
                        if (byte* p{ *(byte**)(obj + o) }) {
                            utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_{:x}.bin", i, j, o), p, 4 * c);
                        }
                    }
                    for (size_t o : { 0x58, 0x68 }) {
                        if (byte* p{ *(byte**)(obj + o) }) {
                            utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_{:x}.bin", i, j, o), p, 0x30);
                            if (byte* u{ *(byte**)(p + 0x18) }) {
                                utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_{:x}_u16.bin", i, j, o), u, 2 * *(uint32_t*)(p + 0x10));
                            }
                            if (void* img{ *(void**)(p + 0x20) }) {
                                BO4JsonWriter ij{};
                                ij.BeginObject();
                                ij.WriteFieldValueXAsset("image", ASSET_TYPE_IMAGE, img);
                                ij.EndObject();
                                ij.WriteToFile(rawDir / std::format("lighting_state{}_obj{}_{:x}_image.json", i, j, o));
                                // the image itself (0x88: format 0x64, w/h/d 0x68, streamed parts 0x30)
                                utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_{:x}_image.bin", i, j, o), img, 0x88);
                            }
                        }
                    }
                    if (byte* p{ *(byte**)(obj + 0x60) }) {
                        utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_60.bin", i, j), p, 0x40);
                        // its data (w x h bytes at 0x8/0xC, the shadow data's fallback while the stream isn't in)
                        if (byte* t{ *(byte**)p }) {
                            utils::WriteFile(rawDir / std::format("lighting_state{}_obj{}_60_data.bin", i, j), t,
                                (size_t)*(uint32_t*)(p + 0x8) * *(uint32_t*)(p + 0xC));
                        }
                    }
                }
            }
        }
        if (asset.streamerworld) {
            utils::WriteFile(rawDir / "streamerworld.bin", asset.streamerworld, 0x100);
        }

        BO4JsonWriter json{};
        json.BeginObject();
        for (const GfxWorldRawArray& arr : gfxWorldRawArrays) {
            const byte* data{ *(const byte* const*)(header + arr.ptr) };
            uint64_t count{ arr.countSize == 8 ? *(const uint64_t*)(header + arr.count) : *(const uint32_t*)(header + arr.count) };
            if (!data && !count) {
                continue;
            }
            std::string key{ std::format("{:x}", arr.ptr) };
            json.WriteFieldNameString(key.c_str());
            json.BeginObject();
            json.WriteFieldValueNumber("count", count);
            json.WriteFieldValueNumber("elemSize", arr.elemSize);
            json.WriteFieldValueBool("data", data != nullptr);
            json.EndObject();
            if (data && count) {
                utils::WriteFile(rawDir / (key + ".bin"), data, (size_t)(count * arr.elemSize));
            }
        }
        json.EndObject();
        json.WriteToFile(rawDir / "arrays.json");
    }

    class GfxWorldWorker : public Worker {

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            GfxWorld& asset{ *(GfxWorld*)ptr };

            const char* name{ hashutils::ExtractPtr(asset.name) };
            if (!name) {
                name = utils::va("maps/hashed/file_%llx.d3dbsp", asset.name.name);
            }

            std::filesystem::path outDir{ opt.m_output / "bo4" / "source" / name };

            std::filesystem::create_directories(outDir);
            DumpGfxWorldRaw(opt, outDir, asset);
            {
                BO4JsonWriter json{};
                json.BeginObject();
                json.WriteFieldValueXHash("name", asset.name);
                json.WriteFieldValueXString("baseName", asset.baseName);
                json.WriteFieldValueVector("mins", asset.mins);
                json.WriteFieldValueVector("maxs", asset.maxs);
                json.WriteFieldValueNumber("checksum", asset.checksum);
                json.WriteFieldValueXAsset("streamerworld", ASSET_TYPE_STREAMERWORLD, asset.streamerworld);
                json.WriteFieldValueXAsset("lighting", ASSET_TYPE_LIGHTING, asset.lighting);
                json.WriteFieldValueXAsset("lutMaterial", ASSET_TYPE_MATERIAL, asset.lutMaterial);
                json.WriteFieldValueXAsset("lutMaterialALT", ASSET_TYPE_MATERIAL, asset.lutMaterialALT);
                json.WriteFieldValueXAsset(
                    "volumeDecalRevealTexture",
                    ASSET_TYPE_IMAGE,
                    asset.volumeDecalRevealTexture
                );
                json.WriteFieldValueXAssetArray("unk550", ASSET_TYPE_IMAGE, ACTS_ARRAYSIZE(asset.unk550), asset.unk550);
                json.WriteFieldValueXAssetArray(
                    "unk1a60",
                    ASSET_TYPE_IMAGE,
                    ACTS_ARRAYSIZE(asset.unk1a60),
                    asset.unk1a60
                );
                {
                    json.WriteFieldNameString("draw");
                    json.BeginObject();

                    GfxWorldDraw& draw{ asset.draw };
                    json.WriteFieldValueNumber("blubIESprofileCNT", draw.blubIESprofileCNT);
                    json.WriteFieldValueXAsset("blubIESprofileIMG", ASSET_TYPE_IMAGE, draw.blubIESprofileIMG);
                    json.WriteFieldValueNumber("cookieCount", draw.cookieCount);
                    json.WriteFieldValueXAsset("cookieArray", ASSET_TYPE_IMAGE, draw.cookieArray);

                    json.EndObject();
                }

                json.EndObject();

                std::filesystem::path outFile{ outDir / "gfx_map.json" };
                if (!json.WriteToFile(outFile)) {
                    LOG_ERROR("Error when dumping {}", outFile.string());
                }
                LOG_OPT_INFO("Dump gfx_map {}", outFile.string());
            }
            {
                BO4JsonWriter json{};
                json.BeginObject();
                GfxWorldDpvsStatic& dpvs{ asset.dpvs };

                json.WriteFieldValueNumber("smodelUpdateFrame", dpvs.smodelUpdateFrame);
                json.WriteFieldValueXAssetArray(
                    "groupLodModels",
                    ASSET_TYPE_GROUPLODMODEL,
                    dpvs.groupLodModelsCount,
                    dpvs.groupLodModels
                );

                if (dpvs.smodelDrawInsts) {
                    json.WriteFieldNameString("smodelDrawInsts");
                    json.BeginArray();
                    for (uint32_t i = 0; i < dpvs.smodelCount; i++) {
                        GfxStaticModelDrawInst& drawInst{ dpvs.smodelDrawInsts[i] };
                        json.BeginObject();
                        json.WriteFieldValueScrString("name", drawInst.name);
                        json.WriteFieldValueXAsset("model", ASSET_TYPE_XMODEL, drawInst.model);
                        json.WriteFieldValueUnknown("drawInst:unk8", drawInst.unk8);
                        json.WriteFieldValueUnknown("drawInst:unk10", drawInst.unk10);
                        json.WriteFieldValueUnknown("drawInst:unk18", drawInst.unk18);
                        json.WriteFieldValueUnknown("drawInst:unk20", drawInst.unk20);
                        json.WriteFieldValueUnknown("drawInst:unk28", drawInst.unk28);
                        json.WriteFieldValueUnknown("drawInst:unk2c", drawInst.unk2c);
                        json.WriteFieldValueNumber("drawInst:unk2e", drawInst.unk2e);
                        json.WriteFieldValueUnknown("drawInst:unk30", drawInst.unk30);
                        json.WriteFieldValueUnknown("drawInst:unk34", drawInst.unk34);

                        json.EndObject();
                    }
                    json.EndArray();
                }

                json.EndObject();

                std::filesystem::path outFile{ outDir / "gfx_map_dpvs.json" };
                if (!json.WriteToFile(outFile)) {
                    LOG_ERROR("Error when dumping {}", outFile.string());
                }
                LOG_OPT_INFO("Dump gfx_map {}", outFile.string());
            }
        }
    };
    utils::MapAdder<GfxWorldWorker, games::bo4::pool::XAssetType, Worker> implgfx{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_GFXWORLD
    };
} // namespace