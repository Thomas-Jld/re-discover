#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <math.h>
#include <cstdlib>
#include "main.h"
#include <chrono>
#include <thread>

#include <sstream>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, PointCloud<float>>, PointCloud<float>, 2 /* dim */> my_kd_tree_t;

void on_message(client *c, FMOD_VECTOR *pos, websocketpp::connection_hdl hdl, message_ptr msg)
{
    // msg is "number1,number2"
    std::string message = msg->get_payload();
    // Get number1
    std::string number1 = message.substr(0, message.find(","));
    // Get number2
    std::string number2 = message.substr(message.find(",") + 1);

    // std::cout << number1 << " " << number2 << std::endl;
    (*pos).x = atof(number1.c_str());
    (*pos).y = atof(number2.c_str());
}

std::vector<int> kdtree_demo(my_kd_tree_t *index, const float query[2], const float search_radius)
{

    // ----------------------------------------------------------------
    // radiusSearch(): Perform a search for the points within search_radius
    // ----------------------------------------------------------------
    {
        std::vector<std::pair<size_t, float>> ret_matches;

        nanoflann::SearchParams params;
        //params.sorted = false;

        const size_t nMatches = (*index).radiusSearch(&query[0], search_radius, ret_matches, params);

        // std::cout << "radiusSearch(): radius=" << search_radius << " -> " << nMatches << " matches\n";
        // for (size_t i = 0; i < nMatches; i++)
        // 	std::cout << "idx["<< i << "]=" << ret_matches[i].first << " dist["<< i << "]=" << ret_matches[i].second << std::endl;
        // std::cout << "\n";

        std::vector<int> idxes;
        for (size_t i = 0; i < nMatches; i++)
            idxes.push_back(ret_matches[i].first);
        return idxes;
    }
}

FMOD_RESULT F_CALLBACK pcmreadcallback(FMOD_SOUND * /*sound*/, void *data, unsigned int datalen)
{
    static float t1 = 0, t2 = 0; // time
    static float v1 = 0, v2 = 0; // velocity
    signed short *stereo16bitbuffer = (signed short *)data;

    for (unsigned int count = 0; count < (datalen >> 2); count++) // >>2 = 16bit stereo (4 bytes per sample)
    {
        *stereo16bitbuffer++ = (signed short)(sin(t1) * 32767.0f); // left channel
        *stereo16bitbuffer++ = (signed short)(sin(t2) * 32767.0f); // right channel

        t1 += 0.01f + v1;
        t2 += 0.0142f + v2;
        v1 += (float)(sin(t1) * 0.002f);
        v2 += (float)(sin(t2) * 0.002f);
    }

    return FMOD_OK;
}

int clean_channels(
    FMOD::Channel **channel,
    int audio_length_ms,
    int buffer_size,
    int chunk_step_ms,
    int *offset,
    std::vector<uint> *playing_queue)
{

    uint chan_pos;
    bool is_playing;
    bool is_virtual;
    for (int j = 0; j < buffer_size; j++)
    {
        chan_pos = 0;
        channel[j]->getPosition(&chan_pos, FMOD_TIMEUNIT_MS);
        channel[j]->isPlaying(&is_playing);
        channel[j]->isVirtual(&is_virtual);
        if (static_cast<int>(chan_pos) - offset[j] >= audio_length_ms || !is_playing || is_virtual || static_cast<int>(chan_pos) <= offset[j])
        {
            if (channel[j] != nullptr)
            {
                channel[j]->stop();
            }
            (*playing_queue)[j] = -1;
            break;
        }
    }

    return 0;
}

int load_sound(
    std::vector<data> *arr,           // array of data struct
    int audio_length_ms,              // lengths of audio to play in ms
    FMOD::Channel **channel,          // pointer to array of FMOD::Channel
    int chunk_step_ms,                // space between chunks in ms
    FMOD_CREATESOUNDEXINFO *exinfo,   // array of exinfo
    std::vector<int> *kd_result,      // result of kdtree
    int *offset,                      // offset from start of file in ms to beginning of chunk
    std::vector<uint> *playing_queue, // array of status for streams; -1 for
                                      // not initialized, else id in kdtree
    FMOD::Sound **s,                  // array of Sound objects
    FMOD::System *system              // pointer to FMOD::System object
)
{
    const char *name;
    FMOD_RESULT result;
    bool is_playing;
    int j;
    float progress = 0.0; // progress bar display
    int barWidth = 70;    // progress bar display

    for (std::vector<int>::iterator res = kd_result->begin(); res != kd_result->end(); ++res)
    {
        // verify if already playing
        if (
            std::find(
                playing_queue->begin(),
                playing_queue->end(),
                *res) == playing_queue->end() &&
            (rand() % 100 < 5 || res == kd_result->end())) // true if absent
        //play and add to playing
        {
            //get empty sound
            for (j = 0; j < playing_queue->size(); j++)
            {
                // channel[j]->isPlaying(&is_playing);
                if ((*playing_queue)[j] == -1)
                    break; // found open slot for loading sound at j
            }

            // std::cout << "load sound at " << j << std::endl;

            (*playing_queue)[j] = *res;

            name = (*arr)[*res].path.data();
            std::cout << "loading " << name << std::endl;
            // Sound loading for audio info retrieval
            s[j]->release();
            result = system->createSound(
                name,
                FMOD_CREATECOMPRESSEDSAMPLE,
                NULL,
                &s[j]);

            if (result != FMOD_OK)
            {
                printf(
                    "FMOD error loading audio! (%d) %s\n",
                    result,
                    FMOD_ErrorString(result));
                printf("%s\n", name);
                exit(-1);
            }

            // Get sound infos
            FMOD_SOUND_TYPE tt;
            FMOD_SOUND_FORMAT ff;
            int num_channels = 0;
            int bits = 0;
            s[j]->getFormat(&tt, &ff, &num_channels, &bits);
            float frequency = 0;
            int priority = 0;
            result = s[j]->getDefaults(&frequency, &priority);
            result = s[j]->release();

            // Fill exinfo data
            exinfo[j].cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
            exinfo[j].numchannels = num_channels;                     /* Number of channels in the sound. */
            exinfo[j].defaultfrequency = static_cast<int>(frequency); /* Default playback rate of sound. */
            offset[j] = chunk_step_ms * (*arr)[*res].chunk;
            exinfo[j].length = static_cast<int>(bits / 8 * (*arr)[*res].chunk * chunk_step_ms * frequency / 1000 * num_channels) //offset
                               + frequency * num_channels * bits / 8 * audio_length_ms / 1000;                                   //audio length
            exinfo[j].format = ff;

            // Load sound for playback
            result = system->createSound(
                name,
                FMOD_3D | FMOD_CREATECOMPRESSEDSAMPLE,
                &exinfo[j],
                &s[j]);

            if (result != FMOD_OK)
            {
                printf(
                    "FMOD error loading audio! (%d) %s\n",
                    result,
                    FMOD_ErrorString(result));
                printf("%s\n", name);
                exit(-1);
            }

            // Assign sound to FMOD::Channel and set to pause immediately
            result = system->playSound(
                s[j],
                NULL,
                true,
                &(channel[j]));

            if (result != FMOD_OK)
            {
                printf(
                    "FMOD error playing sound! (%d) %s\n",
                    result,
                    FMOD_ErrorString(result));
                exit(-1);
            }

            // Change position to offset (i.e. start of chunk)
            result = channel[j]->setPosition(
                chunk_step_ms * (*arr)[*res].chunk,
                FMOD_TIMEUNIT_MS);
            if (result != FMOD_OK)
            {
                printf(
                    "FMOD error setting position! (%d) %s\n",
                    result,
                    FMOD_ErrorString(result));
                // exit(-1);
            }

            // Set sound spatial position
            const FMOD_VECTOR soundPos = {
                float{(*arr)[*res].embedding[0]},
                float{(*arr)[*res].embedding[1]},
                0.0};
            result = channel[j]->set3DAttributes(
                &soundPos,
                nullptr);

            // Add fading
            unsigned long long dspclock;
            int rate;
            FMOD::System *sys;

            result = channel[j]->getSystemObject(&sys);
            result = sys->getSoftwareFormat(&rate, 0, 0);

            result = channel[j]->getDSPClock(0, &dspclock);

            result = channel[j]->addFadePoint(
                dspclock + 0 * rate,
                0.0f);

            result = channel[j]->addFadePoint(
                dspclock + 1 * rate,
                1.0f);

            result = channel[j]->addFadePoint(
                dspclock + 4 * rate,
                1.0f);

            result = channel[j]->addFadePoint(
                dspclock + 5 * rate,
                0.0f);

            // Unpause sound
            result = channel[j]->setPaused(false);
            if (result != FMOD_OK)
            {
                printf(
                    "FMOD error playing audio! (%d) %s\nid: %d\n",
                    result,
                    FMOD_ErrorString(result),
                    j);
                exit(-1);
            }
            break;
        }
    }

    return 0;
}

bool getNextLineAndSplitIntoTokens(
    std::ifstream &str,
    float scale,
    PointCloud<float> *cloud,
    std::vector<data> *arr)
{
    /*
    String processing to transform csv lines into the data structure we need
    Input:
        std::ifstream& str: stream to the csv file with all paths, chunks and
            ids
        float scale: rescale the map data (chunks may to too far apart to
            listen with spatial effects)
        PointCloud<float>* cloud: Output, cloud data to append to for kdtree
            processing
        std::vector<data>* arr: Output, array of type data to append result to

    Output:
        bool: is last line in csv file
    */
    std::vector<std::string> result;
    std::string line;
    std::getline(str, line);

    std::stringstream lineStream(line);
    std::string cell;

    data d;
    int i = 0;
    float x;
    float y;

    while (std::getline(lineStream, cell, ','))
    {
        if (cell == "id")
        {
            return false;
        }
        if (i == 0) // is a path
        {
            d.path = "../mp3/";
            if (cell.front() == '\"')
            {
                d.path.append(cell).append(",");
                std::getline(lineStream, cell, ',');
            }
            d.path.append(cell.substr(0, cell.find("-"))).append(".mp3");
            d.chunk = std::stoi(
                cell.substr(cell.find("-") + 1, std::string::npos));
        }
        else if (i == 1) // is x coordinate
        {
            x = std::stof(cell) * scale;
        }
        else // is y coordinate
        {
            y = std::stof(cell) * scale;
        }
        i++;
    }
    if (cell.empty())
    {
        return true;
    }
    d.embedding = {
        x,
        y};
    cloud->pts.push_back(
        {x,
         y});

    arr->push_back(d);

    return false;
}

int playsound(FMOD_VECTOR *pos)
{
    // Inits
    FMOD_RESULT result;
    FMOD::System *system = NULL;
    PointCloud<float> cloud;
    const char *name;

    std::vector<data> arr;
    float scale = 1.0; // map rescaling
    float chunk_step_ms = 2.5 * 1000.0;
    int buffer_size = 4095; // [1, 4095]

    result = FMOD::System_Create(&system); // Create the main system object.
    if (result != FMOD_OK)
    {
        printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        exit(-1);
    }

    result = system->init(4095, FMOD_INIT_NORMAL, 0); // Initialize FMOD.
    if (result != FMOD_OK)
    {
        printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        exit(-1);
    }

    // Load csv info
    std::ifstream ifs("../data_fmod.csv");
    bool done = false;
    // Parse file until done
    while (!done)
    {
        done = getNextLineAndSplitIntoTokens(ifs, 1, &cloud, &arr);
    }

    // Randomize Seed for kdtree
    srand(static_cast<unsigned int>(time(nullptr)));

    FMOD::Channel *channel[buffer_size];
    memset(channel, 0, sizeof(FMOD::Channel *) * buffer_size);
    FMOD::Sound *s[buffer_size];
    float progress = 0.0;
    int barWidth = 70;
    int j = 0;
    int listener = 0;
    int offset[buffer_size];
    memset(offset, 0, sizeof(int) * buffer_size);
    std::vector<uint> playing_queue(buffer_size, -1);
    FMOD_CREATESOUNDEXINFO exinfo[buffer_size];
    memset(exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO) * buffer_size);

    // construct a kd-tree index:
    my_kd_tree_t index(2 /*dim*/, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10 /* max leaf */));
    index.buildIndex();

#if 0
	// Test resize of dataset and rebuild of index:
	cloud.pts.resize(cloud.pts.size()*0.5);
	index.buildIndex();
#endif

    while (true)
    {
        result = system->set3DListenerAttributes(
            listener,
            pos,
            nullptr,
            nullptr,
            nullptr);

        const float query[2] = {(*pos).x, (*pos).y};
        const float query_radius = 50;
        std::vector<int> kd_result;

        kd_result = kdtree_demo(&index, query, query_radius);
        std::cout << "kdtree done" << std::endl;
        clean_channels(channel, 5000, buffer_size, chunk_step_ms, offset, &playing_queue);
        std::cout << "clean done" << std::endl;
        load_sound(&arr, 5000, channel, chunk_step_ms, exinfo, &kd_result, offset, &playing_queue, s, system);
        std::cout << "load done" << std::endl;
        std::cout << "Position: " << (*pos).x << " " << (*pos).y << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    }

    return 0;
}

int main(int argc, char *argv[])
{
    client c;
    std::string uri = "ws://localhost:9002";

    if (argc == 2)
    {
        uri = argv[1];
    }

    try
    {
        FMOD_VECTOR pos = {1250, -1250, 0}; // Starting point
        std::thread t1(playsound, &pos);

        c.set_access_channels(websocketpp::log::alevel::all);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);
        c.init_asio();
        c.set_message_handler(bind(&on_message, &c, &pos, ::_1, ::_2));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec)
        {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return 0;
        }

        c.connect(con);
        c.run();
    }
    catch (websocketpp::exception const &e)
    {
        std::cout << e.what() << std::endl;
    }
}
