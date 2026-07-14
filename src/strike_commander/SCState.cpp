#include "precomp.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>
//
//  SCState.cpp
//  libRealSpace
//
//  Created by Rémi LEONARD on 09/09/2024.
//  Copyright (c) 2013 Fabien Sanglard. All rights reserved.
//
SCState::SCState() {
    
}
void SCState::Load(std::string filename) {

    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (buffer.size() < 0x24F) {
        std::cerr << "File size is too small: " << filename << std::endl;
        this->Reset();
        return;
    }
    std::string header = std::string(buffer.begin(), buffer.begin() + 7);
    if (header != "SCB1.22") {
        std::cerr << "Invalid file header: " << header << std::endl;
        this->Reset();
        return;
    }
    // Pad short files (saves written by older builds were 0x251 bytes) up to
    // the original game's size, then keep the raw bytes so Save() can
    // round-trip fields we don't parse yet.
    if (buffer.size() < SAVE_FILE_SIZE) {
        buffer.resize(SAVE_FILE_SIZE, 0);
    }
    this->raw_save_buffer = buffer;
    for (int i=0; i<256; i++) {
        this->requierd_flags[i] = buffer[0x0B + i];
    }
    for (int i=0; i<46; i++) {
        this->mission_flyed_success[i] = buffer[0x10B + i];
    }
    /*
        018D - 018E - Amount of money lost
        0189 - 018A - Bank balance
    */
    this->proj_cash = (0 | (buffer[0x18A] << 8) | buffer[0x189]) * 1000;
    this->cash = this->proj_cash;
    this->over_head = (0 | (buffer[0x18E] << 8) | buffer[0x18D]) * 1000;
    
    /*
        024D - 024E - Current Score
    */
    this->score = 0 | (buffer[0x24E] << 8) | buffer[0x24D];
    /*
        0199 - # of Air Kills
        019B - # of ground kills
        Orientation and slot order verified against the in-game KILL BOARD
        of DOS Strike Commander CD at campaign start (air/ground):
        PRIMETIME 30/6, PHOENIX 11/27, BASELINE 12/18, ZORRO 22/13,
        TEX 16/17, VIXEN 19/16, HAWK 33/48 — matching PilotsId order and
        air-kills-first byte layout.
    */
    /*
        01C9 - 01DC - Player Last Name
    */
    this->player_name = std::string(buffer.begin() + 0x1C9, buffer.begin() + 0x1DC);
    this->player_name = this->player_name.substr(0, this->player_name.find('\0'));
    this->player_name.shrink_to_fit();
    /*
        01DD - 01F0 - Player First Name
    */
    this->player_firstname = std::string(buffer.begin() + 0x1DD, buffer.begin() + 0x1F0);
    this->player_firstname = this->player_firstname.substr(0, this->player_firstname.find('\0'));
    this->player_firstname.shrink_to_fit();
    /*
        01F1 - 0204 - Player Callsign
    */
    this->player_callsign = std::string(buffer.begin() + 0x1F1, buffer.begin() + 0x204);
    this->player_callsign = this->player_callsign.substr(0, this->player_callsign.find('\0'));
    this->player_callsign.shrink_to_fit();
    /*
        208 - 21B - WingMan
    */
    this->wingman = std::string(buffer.begin() + 0x208, buffer.begin() + 0x21B);
    this->wingman.shrink_to_fit();
    this->air_kills = buffer[0x199];
    this->ground_kills = buffer[0x19B];
    /* 19D - 1C6 killboard: 7 wingmen (PRIMETIME..HAWK), per pilot
       {alive, ?, air lo, air hi, ground lo, ground hi} */
    for (int i=0; i<7; i++) {
        int alive = buffer[0x19D + i*0x06];
        int air_kills = (buffer[0x19D + i*0x06+3] << 8) | buffer[0x19D + i*0x06+2];
        int ground_kills = (buffer[0x19D + i*0x06+5] << 8) | buffer[0x19D + i*0x06+4];
        this->pilot_roaster[i+1] = alive;
        this->kill_board[i+1][KillBoardType::AIR_KILL] = air_kills;
        this->kill_board[i+1][KillBoardType::GROUND_KILL] = ground_kills;
    }
    this->kill_board[0][KillBoardType::AIR_KILL] = this->air_kills;
    this->kill_board[0][KillBoardType::GROUND_KILL] = this->ground_kills;
    
    auto read16 = [&buffer](size_t offset) -> int16_t {
        return (int16_t)((buffer[offset + 1] << 8) | buffer[offset]);
    };
    this->weapon_inventory = {
        {ID_AIM9J, read16(0x16F)},
        {ID_AIM9M, read16(0x171)},
        {ID_AGM65D, read16(0x173)},
        {ID_LAU3, read16(0x17D)},
        {ID_MK20, read16(0x177)},
        {ID_MK82, read16(0x179)},
        {ID_DURANDAL, read16(0x175)},
        {ID_GBU15, read16(0x17B)},
        {ID_AIM120, read16(0x17F)}
    };
    this->current_mission = buffer[0x08];
    this->mission_id = buffer[0x09];
    this->current_scene = buffer[0x0A];
    this->tune_modifier = buffer[0x24C];
}

void SCState::Save(std::string filename) {
    // Start from the last loaded save so unparsed bytes are preserved;
    // fall back to a zeroed buffer at the original game's file size.
    std::vector<uint8_t> buffer = this->raw_save_buffer;
    if (buffer.size() != SAVE_FILE_SIZE) {
        buffer.assign(SAVE_FILE_SIZE, 0);
    }

    // Write header (original saves end it with 0x01, not 0xFF)
    std::string header = "SCB1.22";
    std::copy(header.begin(), header.end(), buffer.begin());
    buffer[0x07] = 0x01;
    // Current mission, mission ID, and scene
    buffer[0x08] = this->current_mission;
    buffer[0x09] = this->mission_id;
    buffer[0x0A] = this->current_scene;

    // Required flags
    for (int i = 0; i < 256; i++) {
        if (i < this->requierd_flags.size())
            buffer[0x0B + i] = this->requierd_flags[i];
    }

    // Mission flown success
    for (int i = 0; i < 46; i++) {
        if (i < this->mission_flyed_success.size())
            buffer[0x10B + i] = this->mission_flyed_success[i];
    }

    // Money values
    buffer[0x189] = (this->proj_cash / 1000) & 0xFF;
    buffer[0x18A] = ((this->proj_cash / 1000) >> 8) & 0xFF;
    buffer[0x18D] = (this->over_head / 1000) & 0xFF;
    buffer[0x18E] = ((this->over_head / 1000) >> 8) & 0xFF;

    // Kills (0x199 air, 0x19B ground — see Load)
    buffer[0x199] = this->air_kills;
    buffer[0x19B] = this->ground_kills;

    // Kill board: 7 wingmen (PRIMETIME..HAWK), see Load
    for (int i = 0; i < 7; i++) {
        buffer[0x19D + i*0x06] = this->pilot_roaster[i+1];
        // Write air kills
        buffer[0x19D + i*0x06 + 2] = this->kill_board[i+1][KillBoardType::AIR_KILL] & 0xFF;
        buffer[0x19D + i*0x06 + 3] = (this->kill_board[i+1][KillBoardType::AIR_KILL] >> 8) & 0xFF;
        // Write ground kills
        buffer[0x19D + i*0x06 + 4] = this->kill_board[i+1][KillBoardType::GROUND_KILL] & 0xFF;
        buffer[0x19D + i*0x06 + 5] = (this->kill_board[i+1][KillBoardType::GROUND_KILL] >> 8) & 0xFF;
    }

    // Weapon inventory (16-bit little-endian, see Load)
    auto write16 = [&buffer](size_t offset, int16_t value) {
        buffer[offset] = value & 0xFF;
        buffer[offset + 1] = (value >> 8) & 0xFF;
    };
    if (this->weapon_inventory.count(ID_AIM9J)) write16(0x16F, this->weapon_inventory[ID_AIM9J]);
    if (this->weapon_inventory.count(ID_AIM9M)) write16(0x171, this->weapon_inventory[ID_AIM9M]);
    if (this->weapon_inventory.count(ID_AGM65D)) write16(0x173, this->weapon_inventory[ID_AGM65D]);
    if (this->weapon_inventory.count(ID_DURANDAL)) write16(0x175, this->weapon_inventory[ID_DURANDAL]);
    if (this->weapon_inventory.count(ID_MK20)) write16(0x177, this->weapon_inventory[ID_MK20]);
    if (this->weapon_inventory.count(ID_MK82)) write16(0x179, this->weapon_inventory[ID_MK82]);
    if (this->weapon_inventory.count(ID_GBU15)) write16(0x17B, this->weapon_inventory[ID_GBU15]);
    if (this->weapon_inventory.count(ID_LAU3)) write16(0x17D, this->weapon_inventory[ID_LAU3]);
    if (this->weapon_inventory.count(ID_AIM120)) write16(0x17F, this->weapon_inventory[ID_AIM120]);

    // Player names (zero the fixed-width fields first so a shorter name
    // doesn't leave stale characters from the loaded buffer behind)
    auto writeString = [&buffer](const std::string &value, size_t offset, size_t width) {
        std::fill(buffer.begin() + offset, buffer.begin() + offset + width, 0);
        size_t len = value.length() > width ? width : value.length();
        std::copy(value.begin(), value.begin() + len, buffer.begin() + offset);
    };
    writeString(this->player_name, 0x1C9, 19);
    writeString(this->player_firstname, 0x1DD, 19);
    writeString(this->player_callsign, 0x1F1, 19);
    writeString(this->wingman, 0x208, 19);

    buffer[0x24C] = this->tune_modifier;
    // Score
    buffer[0x24D] = this->score & 0xFF;
    buffer[0x24E] = (this->score >> 8) & 0xFF;
    // Bytes past 0x24E are not parsed yet; they are preserved from the
    // loaded save via raw_save_buffer instead of being zeroed.
    // Write to file
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    file.close();
}

void SCState::Reset() {
    this->raw_save_buffer.clear();
    this->requierd_flags.clear();
    this->mission_flyed_success.clear();
    this->missions_flags.clear();
    this->weapon_load_out.clear();
    this->aircraftHooks.clear();
    this->kill_board.clear();
    this->weapon_inventory.clear();
    this->current_mission = 0;
    this->next_mission = 0;
    this->current_scene = 0;
    this->over_head = 0;
    this->proj_cash = 0;
    this->tune_modifier = 1;
}
