#include <bits/types/time_t.h>
#include <cmath>
#include <confini.h>
#include <ctime>
#include <stdlib.h>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>
#include <math.h>
#include <csignal>

inline float map(float x, float in_min, float in_max, float out_min, float out_max){
    return ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

inline float clamp(float x, float min, float max){
    return x < min ? min : x > max ? max : x;
}


enum class Verbosity {
    FATAL = 0,
    ERROR,
    WARNING,
    INFO,
    DEBUG
};

Verbosity global_verbosity = Verbosity::WARNING;

void log(std::string message, Verbosity v){
    time_t tt;
    time (&tt);
    std::string time_str(ctime(&tt));
    time_str.pop_back();

    switch (v) {
    case Verbosity::FATAL:
        std::cout << "\033[31;1m" << time_str << " [F] " << message << "\033[0m\n";
        break;
    case Verbosity::ERROR:
        std::cout << "\033[31m" << time_str << " [E] " << message << "\033[0m\n";
        break;
    case Verbosity::WARNING:
        std::cout << "\033[33m" << time_str << " [W] " << message << "\033[0m\n";
        break;
    case Verbosity::INFO:
        std::cout << "\033[36m" << time_str << " [I] " << message << "\033[0m\n";
        break;
    case Verbosity::DEBUG:
        std::cout << "\033[37m" << time_str << " [D] " << message << "\033[0m\n";
        break;
    }
}

typedef struct Fan {
    std::string* name               = nullptr;
    int* min_pwm_temp               = nullptr;
    int* max_pwm_temp               = nullptr;
    std::string* temp_sensor_path   = nullptr;
    int* min_pwm                    = nullptr;
    int* max_pwm                    = nullptr;
    int* start_pwm                  = nullptr;
    int* stop_pwm                   = nullptr;
    std::string* pwm_control_path   = nullptr;
    std::string* pwm_read_path      = nullptr;
    std::string* pwm_mode_path      = nullptr;
    bool* stopped                   = nullptr;
} Fan;

std::vector<Fan*> fans;

int uinterval=10;
int fanstep=3;

std::string current_section = "null";

static int dispatch(IniDispatch* const dispatch, void* ){
    if(dispatch->type == INI_SECTION) {
        std::string section_candidate = dispatch->data;
        // if ( !(((long long)current_section.find("Fan"))<0) || !(((long long)current_section.find("fan"))<0) ) {
        //     Fan* last = fans.back();
        //     if (fans.size()>0 && (last->max_pwm_temp == nullptr || last->min_pwm_temp == nullptr || 
        //                           last->temp_sensor_path == nullptr || last->pwm_control_path || last->pwm_read_path || 
        //                           last->max_pwm == nullptr || last->min_pwm == nullptr || last->start_pwm == nullptr || last->stop_pwm == nullptr)) {
        //     std::cerr << "Fan \"" << current_section << "\"initialisation incomplete, likely because of invalid or missing values" << "\n" << last;
        //     std::cerr << &last;
        //     }
        // }
        if (!(((long long)section_candidate.find("Fan"))<0) || !(((long long)section_candidate.find("fan"))<0) ) {
            fans.push_back(new Fan());
            fans.back()->name = new std::string(section_candidate);
            fans.back()->stopped = new bool;
        }
        current_section = section_candidate;

    }

    if(dispatch->type == INI_KEY) {
        std::string data = dispatch->data;

        if(current_section == "General"){
            if(data == "interval"){
                uinterval = ini_get_double(dispatch->value) * 1000000;
            }
            if(data == "step"){
                fanstep = ini_get_int(dispatch->value);
            }
        }

        else if (current_section.find("Fan")>0 || current_section.find("fan")>0) {
            if(data == "min_pwm")
                fans.back()->min_pwm = new int(ini_get_int(dispatch->value));
            
            if(data == "max_pwm")
                fans.back()->max_pwm = new int(ini_get_int(dispatch->value));

            if(data == "stop_pwm")
                fans.back()->stop_pwm = new int(ini_get_int(dispatch->value));

            if(data == "start_pwm")
                fans.back()->start_pwm = new int(ini_get_int(dispatch->value));

            if(data == "min_pwm_temp")
                fans.back()->min_pwm_temp = new int(ini_get_int(dispatch->value));

            if(data == "max_pwm_temp")
                fans.back()->max_pwm_temp = new int(ini_get_int(dispatch->value));

            if(data == "pwm_control_path")
                fans.back()->pwm_control_path = new std::string(dispatch->value);

            if(data == "pwm_read_path")
                fans.back()->pwm_read_path = new std::string(dispatch->value);

            if(data == "pwm_mode_path")
                fans.back()->pwm_mode_path = new std::string(dispatch->value);

            if(data == "temp_sensor_path")
                fans.back()->temp_sensor_path = new std::string(dispatch->value);
        }
    }


    return 0;
}

int fanHandler(Fan* fan){
    std::ifstream temp(*fan->temp_sensor_path, std::ios_base::in);
    std::ifstream validate(*fan->pwm_read_path, std::ios_base::in);    
    std::ofstream control(*fan->pwm_control_path, std::ios_base::out);

    char temp_c[16];
    temp.read(temp_c, 16);
    float temp_f = atof(temp_c)/1000;
    temp.close();
    int pwm = clamp(round(map(temp_f, *fan->min_pwm_temp, *fan->max_pwm_temp, *fan->min_pwm, *fan->max_pwm)), *fan->min_pwm, *fan->max_pwm);
    // std::cout << "raw " << *fan->name << " pwm value=" << pwm << "\n";
    if(*fan->stopped){
        if(!(pwm>*fan->start_pwm))
            pwm=0;
        else 
            *fan->stopped = false;
    }

    if(!*fan->stopped){
        if(pwm<*fan->stop_pwm){
            pwm=0;
            *fan->stopped = true;
            // std::cout << "stopped " << *fan->name << "\n";
        }

    }
    // std::cout << "final " << *fan->name << " pwm value=" << pwm << "\n";
    control.write(std::to_string(pwm).c_str(), 16);
    control.close();
    if (control.bad()) {
        log(*fan->name + " write failure, exiting", Verbosity::FATAL);
        return -1;
    }

    char validate_c[16];
    validate.read(validate_c, 16);
    if (!(pwm == atof(validate_c))){
        log(*fan->name + " write validation failure, exiting", Verbosity::FATAL);
        return -1;
    }
    validate.close();
    return 0;
}

void _reset_fans(){
    for (Fan* fan : fans) {
        std::ofstream mode(*fan->pwm_mode_path, std::ios_base::out);
        mode.write("0", 2);
    }

}
void reset_fans(){
    _reset_fans();
}
void reset_fans(int sig){
    log("Exiting because of signal " + std::to_string(sig), Verbosity::FATAL);
    _reset_fans();
    exit(sig);
}

int main(int argc, char** argv){
    // getopt(argc, argv, "v");
    if(argc == 1){
        log("No configuration file provided. exiting", Verbosity::FATAL);
        exit(-1);
    }

    if(load_ini_path(argv[1], INI_DEFAULT_FORMAT, NULL, &dispatch, NULL)){
        log("Config file parsing failed, exiting", Verbosity::FATAL);
        exit(-2);
    }

    std::atexit(&reset_fans);
    signal(SIGINT, &reset_fans);
    signal(SIGSEGV, &reset_fans);
    signal(SIGTERM, &reset_fans);
    signal(SIGABRT, &reset_fans);
    signal(SIGQUIT, &reset_fans);
    signal(SIGKILL, &reset_fans);
    signal(SIGPIPE, &reset_fans);

    for (Fan* fan : fans) {
        std::ofstream mode(*fan->pwm_mode_path, std::ios_base::out);
        mode.write("1", 2);
    }

    start:
        for (Fan* fan : fans) {
            if (fanHandler(fan)!=0) {
                exit(-3);
            }
        }
        usleep(uinterval);
    goto start;
}