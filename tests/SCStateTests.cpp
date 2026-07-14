#include <gtest/gtest.h>
#include "strike_commander/precomp.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

// Layout reference: a real SCB1.22 save written by DOS Strike Commander CD
// (597 bytes). Air/ground orientation and pilot slot order verified against
// the in-game KILL BOARD at campaign start (air/ground): PRIMETIME 30/6,
// PHOENIX 11/27, BASELINE 12/18, ZORRO 22/13, TEX 16/17, VIXEN 19/16,
// HAWK 33/48.
namespace {

struct BoardEntry { int air; int ground; };
// Campaign-start kill board in PilotsId order (slots 1..7)
static const BoardEntry kCampaignStartBoard[7] = {
    {30, 6},  // PRIMETIME
    {11, 27}, // PHOENIX
    {12, 18}, // BASELINE
    {22, 13}, // ZORRO
    {16, 17}, // TEX
    {19, 16}, // VIXEN
    {33, 48}, // HAWK
};

std::vector<uint8_t> makeReferenceSave() {
    std::vector<uint8_t> b(SCState::SAVE_FILE_SIZE, 0);
    const char *header = "SCB1.22";
    std::copy(header, header + 7, b.begin());
    b[0x07] = 0x01;
    b[0x08] = 1;  // current_mission
    b[0x09] = 2;  // mission_id
    b[0x0A] = 29; // current_scene
    // bank balance 3350 (k$), overhead 1500 (k$), 16-bit LE
    b[0x189] = 3350 & 0xFF; b[0x18A] = 3350 >> 8;
    b[0x18D] = 1500 & 0xFF; b[0x18E] = 1500 >> 8;
    // player kills: 2 air, 0 ground
    b[0x199] = 2;
    b[0x19B] = 0;
    // kill board: 7 wingmen, {alive, ?, air lo, air hi, ground lo, ground hi}
    for (int i = 0; i < 7; i++) {
        size_t o = 0x19D + i * 6;
        b[o] = 1;
        b[o + 2] = kCampaignStartBoard[i].air & 0xFF;
        b[o + 3] = (kCampaignStartBoard[i].air >> 8) & 0xFF;
        b[o + 4] = kCampaignStartBoard[i].ground & 0xFF;
        b[o + 5] = (kCampaignStartBoard[i].ground >> 8) & 0xFF;
    }
    // inventory: 38 AIM-9J
    b[0x16F] = 38; b[0x170] = 0;
    // names
    const char *nm = "Mandravillis";  std::copy(nm, nm + 12, b.begin() + 0x1C9);
    const char *fn = "Alexandros";    std::copy(fn, fn + 10, b.begin() + 0x1DD);
    const char *cs = "Alexbeav";      std::copy(cs, cs + 8,  b.begin() + 0x1F1);
    // unparsed bytes that must survive a round-trip
    b[0x18B] = 0x42; // unknown field between balance and overhead
    b[0x250] = 0x01; // tail bytes present in original saves
    b[0x251] = 0xA4;
    return b;
}

std::string writeTempSave(const std::vector<uint8_t> &bytes, const std::string &name) {
    std::string path = testing::TempDir() + name;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return path;
}

std::vector<uint8_t> readFile(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

} // namespace

TEST(SCStateTest, Load_ParsesOriginalSaveFields) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_ref.sav"));

    EXPECT_EQ(state.current_mission, 1);
    EXPECT_EQ(state.mission_id, 2);
    EXPECT_EQ(state.current_scene, 29);
    EXPECT_EQ(state.proj_cash, 3350 * 1000);
    EXPECT_EQ(state.over_head, 1500 * 1000);
    EXPECT_EQ(state.player_name, "Mandravillis");
    EXPECT_EQ(state.player_firstname, "Alexandros");
    EXPECT_EQ(state.player_callsign, "Alexbeav");
    EXPECT_EQ(state.weapon_inventory[ID_AIM9J], 38);
}

TEST(SCStateTest, Load_KillBoardMatchesInGameBoard) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_kills.sav"));

    EXPECT_EQ(state.air_kills, 2);
    EXPECT_EQ(state.ground_kills, 0);
    // All 7 wingmen, in PilotsId order, air-kills-first
    for (int i = 0; i < 7; i++) {
        EXPECT_EQ(state.kill_board[i + 1][KillBoardType::AIR_KILL],
                  kCampaignStartBoard[i].air) << "pilot slot " << i + 1;
        EXPECT_EQ(state.kill_board[i + 1][KillBoardType::GROUND_KILL],
                  kCampaignStartBoard[i].ground) << "pilot slot " << i + 1;
        EXPECT_EQ(state.pilot_roaster[i + 1], 1) << "pilot slot " << i + 1;
    }
    // HAWK is the 7th entry and must not be dropped
    EXPECT_EQ(state.kill_board[PilotsId::HAWK][KillBoardType::AIR_KILL], 33);
    EXPECT_EQ(state.kill_board[PilotsId::HAWK][KillBoardType::GROUND_KILL], 48);
}

TEST(SCStateTest, SaveLoad_RoundTripPreservesKills) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_rt_in.sav"));
    state.air_kills = 5;
    state.ground_kills = 3;
    state.kill_board[2][KillBoardType::AIR_KILL] = 99;
    state.kill_board[2][KillBoardType::GROUND_KILL] = 77;

    std::string out = testing::TempDir() + "scstate_rt_out.sav";
    state.Save(out);

    SCState reloaded;
    reloaded.Load(out);
    EXPECT_EQ(reloaded.air_kills, 5);
    EXPECT_EQ(reloaded.ground_kills, 3);
    EXPECT_EQ(reloaded.kill_board[1][KillBoardType::AIR_KILL], 30);
    EXPECT_EQ(reloaded.kill_board[1][KillBoardType::GROUND_KILL], 6);
    EXPECT_EQ(reloaded.kill_board[2][KillBoardType::AIR_KILL], 99);
    EXPECT_EQ(reloaded.kill_board[2][KillBoardType::GROUND_KILL], 77);
    EXPECT_EQ(reloaded.kill_board[PilotsId::HAWK][KillBoardType::AIR_KILL], 33);
    EXPECT_EQ(reloaded.kill_board[PilotsId::HAWK][KillBoardType::GROUND_KILL], 48);
}

TEST(SCStateTest, Save_MatchesOriginalFormat) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_fmt_in.sav"));

    std::string out = testing::TempDir() + "scstate_fmt_out.sav";
    state.Save(out);
    std::vector<uint8_t> bytes = readFile(out);

    ASSERT_EQ(bytes.size(), SCState::SAVE_FILE_SIZE);
    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 7), "SCB1.22");
    EXPECT_EQ(bytes[0x07], 0x01); // original terminator, not 0xFF
}

TEST(SCStateTest, Save_PreservesUnparsedBytes) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_keep_in.sav"));

    std::string out = testing::TempDir() + "scstate_keep_out.sav";
    state.Save(out);
    std::vector<uint8_t> bytes = readFile(out);

    ASSERT_EQ(bytes.size(), SCState::SAVE_FILE_SIZE);
    EXPECT_EQ(bytes[0x18B], 0x42);
    EXPECT_EQ(bytes[0x250], 0x01);
    EXPECT_EQ(bytes[0x251], 0xA4);
}

TEST(SCStateTest, Save_ShorterNameLeavesNoStaleCharacters) {
    SCState state;
    state.Load(writeTempSave(makeReferenceSave(), "scstate_name_in.sav"));
    state.player_name = "Ng"; // shorter than "Mandravillis"

    std::string out = testing::TempDir() + "scstate_name_out.sav";
    state.Save(out);

    SCState reloaded;
    reloaded.Load(out);
    EXPECT_EQ(reloaded.player_name, "Ng");
}

TEST(SCStateTest, Load_AcceptsLegacyShortSaves) {
    // Saves written by builds before this fix were 0x251 bytes
    std::vector<uint8_t> shortSave = makeReferenceSave();
    shortSave.resize(0x251);

    SCState state;
    state.Load(writeTempSave(shortSave, "scstate_legacy.sav"));
    EXPECT_EQ(state.player_callsign, "Alexbeav");
    EXPECT_EQ(state.proj_cash, 3350 * 1000);
}

TEST(SCStateTest, Load_RejectsBadHeaderAndTruncatedFiles) {
    std::vector<uint8_t> bad = makeReferenceSave();
    bad[0] = 'X';
    SCState state;
    state.Load(writeTempSave(bad, "scstate_bad.sav"));
    EXPECT_EQ(state.proj_cash, 0); // Reset() ran

    std::vector<uint8_t> tiny(0x100, 0);
    SCState state2;
    state2.Load(writeTempSave(tiny, "scstate_tiny.sav"));
    EXPECT_EQ(state2.proj_cash, 0);
}
