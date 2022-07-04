#include <cstdio>
#include <vector>
#include <map>
#include <array>
#include <string>
#include <sstream>
#include <functional>

// dependencies
#include <nlohmann/json.hpp>
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

using json = nlohmann::json;

float EaseSineIn(float t, float b, float c, float d) { t /= d; return (c*t*t*t + b); }

struct BeatTime {
    Sound sound;
    Color color;
    unsigned int repeats = 1;
    float showForSeconds;
    bool playSound = false;
};

void DrawBeatTimes(std::map<float, BeatTime> &beatTimes, float showInterval) {
    for (auto &[ratio, bt] : beatTimes)
    {
        if (bt.playSound)
        {
            PlaySound(bt.sound);
            bt.playSound = false;
        }

        if (bt.showForSeconds > 0)
        {
            bt.showForSeconds -= GetFrameTime();
            DrawLineEx(
                { static_cast<float>(GetScreenWidth()) / ratio, 0 },
                { static_cast<float>(GetScreenWidth()) / ratio, static_cast<float>(GetScreenHeight()) },
                EaseSineIn(bt.showForSeconds, 0, 30, showInterval), bt.color);
        }
    }
}

void Init()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "polyrhythm mf");
    InitAudioDevice();

    SetTargetFPS(480);

    if (!IsAudioDeviceReady())
    {
        CloseWindow();
        throw std::runtime_error("audio device did not initialize properly");
    }

    SetMasterVolume(0.25f);
}

void Deinit()
{
    CloseAudioDevice();
    CloseWindow();
}

std::string GetRatioString(std::vector<float> ratios)
{
    std::stringstream rStrStream;

    for (auto r : ratios)
        rStrStream << r << ":";

    std::string ratioString = rStrStream.str();
    ratioString.pop_back();

    return ratioString;
}

std::map<float, BeatTime> LoadBeatTimeMap(std::vector<float> ratios, std::string soundPath)
{
    std::map<float, BeatTime> beatTimes {};

    for (auto ratio : ratios)
    {
        BeatTime bt {};
        bt.sound = LoadSound(soundPath.c_str());
        bt.color = { static_cast<unsigned char>(GetRandomValue(0, 255)),
                     static_cast<unsigned char>(GetRandomValue(0, 255)),
                     static_cast<unsigned char>(GetRandomValue(0, 255)), 128 };
        SetSoundPitch(bt.sound, 1.f / ratio * 4);
        beatTimes[ratio] = bt;
    }

    return beatTimes;
}

void UnloadBeatTimeMap(std::map<float, BeatTime> &beatTimes)
{
    for (auto &[ratio, bt] : beatTimes)
        UnloadSound(bt.sound);
    
    beatTimes.clear();
}

json LoadJSON(std::string path)
{
    unsigned char *buffer = nullptr;
    unsigned int *length = new unsigned int;

    buffer = LoadFileData(path.c_str(), length);
    std::string out = std::string(reinterpret_cast<char*>(buffer));
    out.resize(*length);
    delete length;
    UnloadFileData(buffer);

    return json::parse(out);
}

int main(int argc, char **argv)
{
    Init();

    std::vector<float> ratios { 7, 3 };
    std::string ratioString = GetRatioString(ratios);

    int bpm = 0;
    float beat = 0;
    bool mute = true;

    std::map<float, BeatTime> beatTimes {};

    std::function updateBeatData = [&](json value) -> void {
        value["bpm"].get_to(bpm);
        value["ratios"].get_to<std::vector<float>>(ratios);
        ratioString = GetRatioString(ratios);

        beat = 60.f / bpm;

        UnloadBeatTimeMap(beatTimes);
        beatTimes = LoadBeatTimeMap(ratios, "ping.wav");
    };

    if (argv[1] != nullptr)
    {
        std::string fileStr = std::string(argv[1]);
        if (std::string(fileStr).substr(fileStr.size() - 5) == ".json")
            updateBeatData(LoadJSON(fileStr));
    }
    std::string statusText = "No config loaded.";

    printf("bpm: %d\n", bpm);
    while (!WindowShouldClose())
    {
        static float showInterval = 0.01f;
        static float currentBeatTime = 0.f;

        if (IsFileDropped())
        {
            int *count = new int;
            std::string droppedFile { GetDroppedFiles(count)[0] };
            delete count;
            ClearDroppedFiles();

            // check if dropped file path ends with .json
            if (droppedFile.substr(droppedFile.size() - 5) == ".json")
            {
                try {
                    updateBeatData(LoadJSON(droppedFile));
                } catch (std::exception &e) {
                    statusText = "Error: " + std::string { e.what() };
                }
            }
            else
            {
                statusText = "Invalid file format.";
            }
        }


        if (beatTimes.empty())
        {

            BeginDrawing();
                ClearBackground(WHITE);
                DrawText(statusText.c_str(), GetScreenWidth() / 2 - MeasureText(statusText.c_str(), 20) / 2, GetScreenHeight() / 2 - 10, 20, LIGHTGRAY);
            EndDrawing();

            continue;
        }

        currentBeatTime += GetFrameTime();

        for (auto &[ratio, bt] : beatTimes)
        {
            if (currentBeatTime > beat / ratio * static_cast<float>(bt.repeats))
            {
                // printf("ratio: %d; bt.repeats: %lld\n", ratio, bt.repeats);
                bt.showForSeconds = showInterval;
                if (!mute) bt.playSound = true;
                bt.repeats++;
                if (bt.repeats > ratio) bt.repeats = 1;
            }
        }

        if (currentBeatTime > beat)
            currentBeatTime = 0.f;

        BeginDrawing();
            ClearBackground(WHITE);
            DrawBeatTimes(beatTimes, showInterval);

            showInterval = GuiSlider({ 0, 0, static_cast<float>(GetScreenWidth()), 20 }, nullptr, nullptr, showInterval, 0.01f, 1.f);
            mute = GuiCheckBox({ 0, 30, 20, 20 }, "Mute", mute);

            DrawText(ratioString.c_str(), GetScreenWidth() - MeasureText(ratioString.c_str(), 50) - 10, GetScreenHeight() - 55, 50, LIGHTGRAY);
            DrawText(TextFormat("%.2f", currentBeatTime), 10, GetScreenHeight() - 90, 20, BLACK);
            DrawText(TextFormat("%.2f", showInterval), 10, GetScreenHeight() - 60, 20, BLACK);
            DrawFPS(10, GetScreenHeight() - 30);
        EndDrawing();
    }

    UnloadBeatTimeMap(beatTimes);
    Deinit();
}