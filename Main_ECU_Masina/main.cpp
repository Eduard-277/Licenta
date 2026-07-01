#include <Arduino.h>
#include <Bluepad32.h>

#define RX1PIN 16
#define TX1PIN 17

#define AWAKEPIN 5

#define KEYEXPR_CRANE_ELECTROMAGNET "crane/electromagnet"
#define KEYEXPR_CRANE_ALLOW_REMOTE "crane/allow_remote"
#define KEYEXPR_CRANE_SPEED_MODE "crane/speed_mode"
#define KEYEXPR_CRANE_MH_CONTINUOUS "crane/motor_horizontal/continuous"
#define KEYEXPR_CRANE_MV_CONTINUOUS "crane/motor_vertical/continuous"
#define KEYEXPR_CRANE_FAULT "crane/fault"
#define KEYEXPR_CRANE_PROCESS_STATUS "crane/process_status"
#define KEYEXPR_CRANE_MH_LIMITS "crane/motor_horizontal/limits"
#define KEYEXPR_CRANE_MV_LIMITS "crane/motor_vertical/limits"
#define KEYEXPR_CRANE_CARGO "crane/cargo"

#define KEYEXPR_CONTROLLER_DPAD_UP "car/controller/dpad/up_req"
#define KEYEXPR_CONTROLLER_DPAD_DOWN "car/controller/dpad/down_req"
#define KEYEXPR_CONTROLLER_DPAD_LEFT "car/controller/dpad/left_req"
#define KEYEXPR_CONTROLLER_DPAD_RIGHT "car/controller/dpad/right_req"
#define KEYEXPR_CONTROLLER_ELEC_REQ "car/controller/electromagnet_req"
#define KEYEXPR_CONTROLLER_UNLOAD_REQ "car/controller/unload_req"
#define KEYEXPR_CONTROLLER_LOAD_REQ "car/controller/load_req"
#define KEYEXPR_CONTROLLER_REM_REQ "car/controller/remote_req"
#define KEYEXPR_CAR_MOVEMENT "car/movement"
#define KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT "car/conn_req_timeout"
#define KEYEXPR_CAR_CONTROLLER_STATUS "car/controller/status"
#define KEYEXPR_CAR_CONTROLLER_SPEED_REQ "car/controller/speed_req"
#define KEYEXPR_CAR_FAULT "car/fault"

#define TELEMATIC_ECU_ONLINE "telematic_online"
#define MAIN_ECU_ONLINE "main_online"

#define REFRESH_VALUES "refresh"

String command;
String serial_name;
String serial_value;
int separator_index=0;

int serial_fail_cnt=0;
int telematic_ecu_status=0;

int controller_status=0;
int electromagnet_timer=-1;
int dpad_timer=-1;
int change_timer_mode=-1;
int budget = 2;
int crane_electromagnet = 0;
int crane_allow_remote = 0;
int crane_speed_mode = 0;
int crane_motor_horizontal_continuous = 0;
int crane_motor_vertical_continuous = 0;
int crane_fault = 0;
int crane_process_status = 0;
int crane_cargo=0;
int crane_motor_horizontal_limits = 0;
int crane_motor_vertical_limits = 0;

int process_started=0;
int electromagnet_req = 0;
int unload_req = 0;
int load_req = 0;
int remote_req = 0;

enum Color
{
    OFF,
    GREEN,
    RED,
    BLUE,
    PINK,
    PURPLE,
    ORANGE,
    CYAN,
    WHITE,
    YELLOW
};

enum Rumble
{
    OFF_RUMBLE,
    SHORT,
    MEDIUM,
    LONG
};

enum Movement
{
    IDLE    =0,
    FORWARD =1,
    REVERSE =2,
    LEFT    =3,
    RIGHT   =4
};

enum Movement prev_car_movement = IDLE, car_movement = IDLE;
enum Color selected_color = OFF, prev_selected_color = OFF;
int selected_leds = 0, prev_selected_leds = 0;
int selected_leds_req=0;
enum Rumble play_rumble = OFF_RUMBLE;

unsigned long lastTimeoutLoop = 0;
unsigned long lastTimeoutGet = 0;
unsigned long lastTimeoutPub = 0;

int flag_get_info = 0;
int cnt_get = 0;
int pub_remote_req = 0, flag_pub_remote_req = 0;
int pub_dpad_up = 0, flag_pub_dpad_up = 0;
int pub_dpad_down = 0, flag_pub_dpad_down = 0;
int pub_dpad_left = 0, flag_pub_dpad_left = 0;
int pub_dpad_right = 0, flag_pub_dpad_right = 0;
int pub_timeout_connection = 0, flag_pub_timeout_connection = 0;

int prev_triangle=0,triangle=0;
int prev_cross=0,cross=0;
int prev_circle=0,circle=0;
int prev_square=0,square=0;
int prev_crane_speed_mode=0;
int prev_r1=0,r1=0;
int prev_l1=0,l1=0;
int animation_ongoing = 0;
int reset_timer = 0;
int connection_unsuccesful = 0;
int connection_succesful = 0;
int timeout_connection = -1;
int unsuccesful_cnt = 4;
int succesful_cnt = 4;
int prev_remote_req = 0;
int prev_dpad_up = 0;
int prev_dpad_down = 0;
int prev_dpad_left = 0;
int prev_dpad_right = 0;
int dpad_up = 0;
int dpad_down = 0;
int dpad_left = 0;
int dpad_right = 0;
char buffer[32];

unsigned long lastTimeoutTele=0;
unsigned long previousMillis = 0;
unsigned long interval = 30000;

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

int ENB = 14; // speed
int IN4 = 27; // dir 1
int IN3 = 26; // dir 2
int motorSpeedB = 0;

int ENA = 32; // speed
int IN2 = 25; // dir 1
int IN1 = 33; // dir 2
int motorSpeedA = 0;

void onConnectedController(ControllerPtr ctl)
{
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (myControllers[i] == nullptr)
        {
            Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                          properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot)
    {
        Serial.println("CALLBACK: Controller connected, but could not found empty slot");
    }
    controller_status=1;
    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_STATUS,controller_status);
}

void onDisconnectedController(ControllerPtr ctl)
{
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (myControllers[i] == ctl)
        {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }

    if (!foundController)
    {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }

    controller_status=0;
    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_STATUS,controller_status);
}

void processGamepad(ControllerPtr ctl)
{

    if(ctl->miscCapture() && load_req==0 && unload_req==0)
    {
        remote_req = 0;
        electromagnet_req = 0;
        selected_leds_req = 0;
        unsuccesful_cnt = 0;
        timeout_connection = -1;
        connection_unsuccesful = 1;
        selected_leds = 0;
        reset_timer = 1;
        dpad_timer=-1;
        electromagnet_timer=-1;
        change_timer_mode = -1;
        ctl->disconnect();
    }

    if (ctl->miscSelect() && remote_req == 1 && load_req==0 && unload_req==0)
    {
        electromagnet_req = 0;
        remote_req = 0;
        selected_leds = 0;
        selected_leds_req = 0;
        timeout_connection = -1;
        change_timer_mode = -1;
        dpad_timer=-1;
        electromagnet_timer=-1;
        reset_timer = 1;
    }

    if (ctl->miscStart() && remote_req == 0)
    {
        remote_req = 1;
        timeout_connection = 400;
        connection_succesful = 0;
        connection_unsuccesful = 0;
    }

    if (remote_req == 1)
    {
        if (crane_allow_remote == 1)
        {
            if (connection_succesful == 0)
            {
                succesful_cnt = 0;
                timeout_connection = -1;
                connection_succesful = 1;
                reset_timer = 1;
                change_timer_mode = -1;
                electromagnet_timer=-1;
                dpad_timer=-1;

                switch (crane_speed_mode)
                {
                case 1:
                    selected_leds = 1;
                    selected_leds_req = 1;
                    break;
    
                case 2:
                    selected_leds = 2;
                    selected_leds_req = 2;
                    break;
    
                case 4:
                    selected_leds = 3;
                    selected_leds_req = 3;
                    break;
    
                default:
                    break;
                }

                if(selected_leds_req==3)
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
                else
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);
            }

        }
        else if (crane_allow_remote == 2 && connection_unsuccesful == 0)
        {
            remote_req = 0;
            unsuccesful_cnt = 0;
            timeout_connection = -1;
            connection_unsuccesful = 1;
            selected_leds = 0;
            reset_timer = 1;
            change_timer_mode = -1;
            electromagnet_timer=-1;
            dpad_timer=-1;
            load_req=0;
            unload_req=0;
            process_started=0;
            Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
            Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req);
            
        }
    }

    if (remote_req == 1 && crane_allow_remote == 1 && (load_req==1 || unload_req==1))
    {
        if(selected_color==YELLOW)
            selected_color=OFF;
        else
            selected_color=YELLOW;

        switch(crane_process_status)
        {
            case 1:
                if(process_started==0)
                {
                    process_started=1;
                    succesful_cnt = 0;
                    selected_leds = 0;
                }
            break;

            case 2:
                selected_leds = 1;
            break;

            case 3:
                selected_leds = 2;
            break;

            case 4:
                selected_leds = 3;
            break;

            case 5:  
            break;

            case 6:
                selected_leds = 4;
            break;

            case 7:  
            break;

            case 8:
                process_started=0;
                succesful_cnt = 0;
                load_req=0;
                unload_req=0;
                switch (crane_speed_mode)
                {
                case 1:
                    selected_leds = 1;
                    selected_leds_req = 1;
                    break;
    
                case 2:
                    selected_leds = 2;
                    selected_leds_req = 2;
                    break;
    
                case 4:
                    selected_leds = 3;
                    selected_leds_req = 3;
                    break;
    
                default:
                    break;
                }
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req);
            break;

            // case 9:
            //     process_started=0;
            //     unsuccesful_cnt = 0;
            //     load_req=0;
            //     unload_req=0;
            //     switch (crane_speed_mode)
            //     {
            //     case 1:
            //         selected_leds = 1;
            //         selected_leds_req = 1;
            //         break;
    
            //     case 2:
            //         selected_leds = 2;
            //         selected_leds_req = 2;
            //         break;
    
            //     case 4:
            //         selected_leds = 3;
            //         selected_leds_req = 3;
            //         break;
    
            //     default:
            //         break;
            //     }
            //     Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
            //     Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req);
            // break;
        }
    }
    else
    if (ctl->axisX() <= -25)
    {
        motorSpeedA = map(ctl->axisX(), -25, -508, 70, 255);
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        analogWrite(ENA, 255);

        motorSpeedB = map(ctl->axisX(), -25, -508, 70, 255);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        analogWrite(ENB, 255);

        selected_color = ORANGE;
        play_rumble = SHORT;
        car_movement = LEFT;
    }
    else if (ctl->axisX() >= 25)
    {
        motorSpeedB = map(ctl->axisX(), 25, 512, 70, 255);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, 255);

        motorSpeedA = map(ctl->axisX(), 25, 512, 70, 255);
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, 255);

        selected_color = ORANGE;
        play_rumble = SHORT;
        car_movement = RIGHT;
    }
    else if (ctl->throttle() > 0 && ctl->brake() <= 0)
    {
        motorSpeedB = map(ctl->throttle(), 0, 1020, 70, 255);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        analogWrite(ENB, 255);

        motorSpeedA = map(ctl->throttle(), 0, 1020, 70, 255);
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, 255);

        selected_color = BLUE;
        play_rumble = SHORT;
        car_movement = FORWARD;
    }
    else if (ctl->throttle() <= 0 && ctl->brake() > 0)
    {
        motorSpeedB = map(ctl->brake(), 0, 1020, 70, 255);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, 255);

        motorSpeedA = map(ctl->brake(), 0, 1020, 70, 255);
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        analogWrite(ENA, 255);

        selected_color = PURPLE;
        play_rumble = SHORT;
        car_movement = REVERSE;
    }
    else
    {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, 0);

        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, 0);

        play_rumble = OFF_RUMBLE;
        car_movement = IDLE;    

        if (remote_req == 1 && crane_allow_remote == 1)
        {
            dpad_up = (ctl->dpad() & DPAD_UP);
            dpad_down = (ctl->dpad() & DPAD_DOWN) >> 1;
            dpad_left = (ctl->dpad() & DPAD_LEFT) >> 3;
            dpad_right = (ctl->dpad() & DPAD_RIGHT) >> 2;
            r1 = ctl->r1();
            l1 = ctl->l1();
            square = ctl->x();
            circle = ctl->b();
            cross = ctl->a();
            triangle = ctl->y();

            if(prev_circle==0 && circle==1 && square==0)
            {
                unload_req=1;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req); 
            }

            if(prev_square==0 && square==1 && circle==0)
            {
                load_req=1;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
            }

            if (prev_dpad_up != dpad_up)
            {
                dpad_timer=20;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_UP,dpad_up);
            }
            if (prev_dpad_down != dpad_down)
            {
                dpad_timer=20;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_DOWN,dpad_down);
            }
            if (prev_dpad_left != dpad_left)
            {
                dpad_timer=20;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_LEFT,dpad_left);
            }
            if (prev_dpad_right != dpad_right)
            {
                dpad_timer=20;
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_RIGHT,dpad_right);
            }

            if (dpad_up != 0 || dpad_down != 0 || dpad_left != 0 || dpad_right != 0)
            {
                selected_color = GREEN;

                if(dpad_right)
                {
                    
                    if((crane_motor_horizontal_continuous!=1 && dpad_timer==-1) || crane_motor_horizontal_limits==2)
                    {
                        unsuccesful_cnt = 0;
                    }
                }
                
                if(dpad_left)
                {
                    if((crane_motor_horizontal_continuous!=2 && dpad_timer==-1) || crane_motor_horizontal_limits==1)
                    {
                        unsuccesful_cnt = 0;
                    }
                }
                
                if(dpad_up)
                {
                    if((crane_motor_vertical_continuous!=2 && dpad_timer==-1) || crane_motor_vertical_limits==2)
                    {
                        unsuccesful_cnt = 0;
                    }
                }
                
                if(dpad_down)
                {
                    if((crane_motor_vertical_continuous!=1 && dpad_timer==-1) || crane_motor_vertical_limits==1)
                    {
                        unsuccesful_cnt = 0;
                    }

                }
            }
            else
            {
                selected_color = PINK;
                dpad_timer=-1;
            }

            if(prev_cross==0 && cross==1)
            {
                electromagnet_req = 1;
                electromagnet_timer=20; 
                Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_ELEC_REQ,electromagnet_req);
            }

            if(prev_triangle==0 && triangle==1)
            {
                if(crane_cargo==0)
                {
                    electromagnet_req = 0;
                    electromagnet_timer=20; 
                    Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_ELEC_REQ,electromagnet_req);
                }
                else
                    unsuccesful_cnt = 0;
            }

            if(prev_r1==0 && r1==1)
            {
                if(selected_leds_req<3)
                    selected_leds_req+=1;
                else
                    selected_leds_req=1;

                change_timer_mode=20;
                if(selected_leds_req==3)
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
                else
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);
            }

            if(prev_l1==0 && l1==1)
            {
                if(selected_leds_req>1)
                    selected_leds_req-=1;
                else
                    selected_leds_req=3;
                
                change_timer_mode=20;
                if(selected_leds_req==3)
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
                else
                    Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);
            }

            if(prev_crane_speed_mode != crane_speed_mode)
            {
                switch (crane_speed_mode)
                {
                case 1:
                    selected_leds = 1;

                    break;
        
                case 2:
                    selected_leds = 2;

                    break;
        
                case 4:
                    selected_leds = 3;

                    break;
        
                default:
                    break;
                } 

            }


        }
        else
        {
            selected_color = WHITE;
        }
    }

    if (reset_timer == 1)
    {
        flag_pub_timeout_connection = 2;
        reset_timer = 0;
    }

    if (timeout_connection != -1)
    {
        flag_pub_timeout_connection = 1;
        if (timeout_connection % 2 == 0)
            selected_color = CYAN;
        else
            selected_color = OFF;

        --timeout_connection;

        // if (timeout_connection == 0)
        // {
        //     Serial1.printf("%s/\n",REFRESH_VALUES);
        // }

        if (timeout_connection == -1)
        {
            flag_pub_timeout_connection = 2;
            remote_req = 0;
            unsuccesful_cnt = 0;
            selected_leds = 0;
            selected_leds_req = 0;
            electromagnet_req = 0;
        }
    }

    if(electromagnet_req == crane_electromagnet && electromagnet_timer!=-1)
    {
        succesful_cnt=0;
        electromagnet_timer=-1; 
    }

    if(electromagnet_timer==0)
    {
        unsuccesful_cnt=0;
        electromagnet_req=crane_electromagnet;
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_ELEC_REQ,electromagnet_req);
    }

    if(selected_leds_req == selected_leds && change_timer_mode!=-1)
    {
        succesful_cnt=0;
        change_timer_mode=-1;
    }

    if(change_timer_mode==0)
    {
        unsuccesful_cnt=0;
        selected_leds_req=selected_leds;
        if(selected_leds_req==3)
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
        else
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);
    }

    if(telematic_ecu_status==0)
    {
        ++serial_fail_cnt;
        if(serial_fail_cnt>=3)
            selected_color = RED;
        if(serial_fail_cnt>200)
            serial_fail_cnt=3;
    }

    if (succesful_cnt < 4)
    {
        switch (succesful_cnt)
        {
        case 0:
            flag_get_info = 1;
            animation_ongoing = 1;
            selected_color = GREEN;
            play_rumble = MEDIUM;
            break;

        case 1:
            selected_color = OFF;
            play_rumble = MEDIUM;
            break;

        case 2:
            selected_color = GREEN;
            play_rumble = MEDIUM;
            break;

        case 3:
            animation_ongoing = 0;
            selected_color = OFF;
            play_rumble = OFF_RUMBLE;

            if(load_req==1 || unload_req==1)
                selected_color = YELLOW;

            break;

        default:
            break;
        }
        ++succesful_cnt;
    }

    if (unsuccesful_cnt < 4)
    {
        switch (unsuccesful_cnt)
        {
        case 0:
            animation_ongoing = 1;
            selected_color = RED;
            play_rumble = MEDIUM;
            break;

        case 1:
            selected_color = OFF;
            play_rumble = MEDIUM;
            break;

        case 2:
            selected_color = RED;
            play_rumble = MEDIUM;
            break;

        case 3:
            animation_ongoing = 0;
            selected_color = OFF;
            play_rumble = OFF_RUMBLE;

            if(load_req==1 || unload_req==1)
                selected_color = YELLOW;

            break;

        default:
            break;
        }
        ++unsuccesful_cnt;
    }

    if (prev_selected_color != selected_color)
        switch (selected_color)
        {
        case GREEN:
            ctl->setColorLED(0, 255, 0);
            break;
        case RED:
            ctl->setColorLED(255, 0, 0);
            break;
        case BLUE:
            ctl->setColorLED(0, 0, 255);
            break;
        case PINK:
            ctl->setColorLED(127, 0, 255);
            play_rumble = OFF_RUMBLE;
            break;
        case PURPLE:
            ctl->setColorLED(255, 29, 206);
            break;
        case ORANGE:
            ctl->setColorLED(255, 165, 0);
            break;
        case CYAN:
            ctl->setColorLED(0, 255, 240);
            break;
        case WHITE:
            ctl->setColorLED(255, 255, 255);
            break;
        case YELLOW:
            ctl->setColorLED(255, 206, 27);
            break;
        case OFF:
        default:
            ctl->setColorLED(0, 0, 0);
            break;
        }

    switch (play_rumble)
    {
    case SHORT:
        ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */, 0x40 /* strongMagnitude */);
        break;
    case MEDIUM:
        ctl->playDualRumble(0 /* delayedStartMs */, 1500 /* durationMs */, 0x80 /* weakMagnitude */, 0x40 /* strongMagnitude */);
        break;
    case LONG:
        break;
    case OFF_RUMBLE:
    default:
        ctl->playDualRumble(0 /* delayedStartMs */, 0 /* durationMs */, 0x80 /* weakMagnitude */, 0x40 /* strongMagnitude */);
        break;
    }

    if (prev_selected_leds != selected_leds)
    {
        ctl->setPlayerLEDs(selected_leds);
    }

    
    if (prev_remote_req != remote_req)
    {
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_REM_REQ,remote_req);
    }

    switch(flag_pub_timeout_connection)
    {
        case 1:
        Serial1.printf("%s/%.2f\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,(timeout_connection * 150.0) / 1000.0);
        flag_pub_timeout_connection=0;
        break;
        case 2:
        Serial1.printf("%s/%.2f\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,0.0);
        flag_pub_timeout_connection=0;
        break;

        default:
        break;
    }

    if(prev_car_movement != car_movement)
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_MOVEMENT,car_movement);

    // if(change_timer_mode==20)
    //     Serial1.printf("%s/\n",REFRESH_VALUES);

    if(change_timer_mode!=-1)
        --change_timer_mode;

    if(electromagnet_timer!=-1)
        --electromagnet_timer;

    if(dpad_timer!=-1)
        --dpad_timer;
    

    prev_square = square;
    prev_triangle = triangle;
    prev_cross = cross;
    prev_circle = circle;
    prev_l1 = l1;
    prev_r1 = r1;
    prev_crane_speed_mode = crane_speed_mode;
    prev_car_movement = car_movement;
    prev_selected_leds = selected_leds;
    prev_selected_color = selected_color;
    prev_remote_req = remote_req;
    prev_dpad_up = dpad_up;
    prev_dpad_down = dpad_down;
    prev_dpad_left = dpad_left;
    prev_dpad_right = dpad_right;
}

void processControllers()
{
    for (auto myController : myControllers)
    {
        if (myController && myController->isConnected() && myController->hasData())
        {
            if (myController->isGamepad())
            {
                processGamepad(myController);
            }
            else
            {
                Serial.println("Unsupported controller");
            }
        }
    }
}


void setup()
{

    pinMode(AWAKEPIN, INPUT_PULLUP);
    pinMode(ENB, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ENA, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);

    Serial.begin(115200);
    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t *addr = BP32.localBdAddress();
    Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);

    // Serial1.begin(115200,SERIAL_8N1,RX1PIN,TX1PIN);
    // Serial1.printf("%s/\n",TELEMATIC_ECU_ONLINE);
    lastTimeoutTele=millis();
    // Serial1.printf("%s/\n",REFRESH_VALUES);
}

unsigned long lastRefresh=0,lastUpdate=0;
int telematic_awake_status=0,switch_awake_once=0,switch_sleep_once=0;
int prev_fault=-1,fault=-1;

void loop()
{

    telematic_awake_status=digitalRead(AWAKEPIN);

    if(telematic_awake_status==0 && switch_awake_once==0)
    {
        Serial1.begin(115200,SERIAL_8N1,RX1PIN,TX1PIN);

        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_UP,dpad_up);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_DOWN,dpad_down);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_LEFT,dpad_left);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_RIGHT,dpad_right);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_ELEC_REQ,electromagnet_req);
        if(selected_leds_req==3)
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
        else
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_REM_REQ,remote_req);
        Serial1.printf("%s/%.2f\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,0.0);
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_MOVEMENT,car_movement);
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_STATUS,controller_status);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req);
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_FAULT,fault);

        switch_awake_once=1;
        switch_sleep_once=0;
        Serial.printf("%d/0\n",telematic_awake_status);
    }
    
    if(telematic_awake_status==1 && switch_sleep_once==0)
    {
        switch_sleep_once=1;
        switch_awake_once=0;
        Serial1.end();
        pinMode(RX1PIN, INPUT);
        pinMode(TX1PIN, INPUT);
        Serial.printf("%d/1\n",telematic_awake_status);
    }

    if (millis() - lastTimeoutLoop >= 150)
    {
        lastTimeoutLoop = millis();
        if (BP32.update())
            processControllers();
    }

    if (millis() - lastTimeoutTele >= 6000)
    {
        lastTimeoutTele = millis();
        
        telematic_ecu_status=0;
        Serial1.printf("%s/\n",TELEMATIC_ECU_ONLINE);
    }

    if (millis() - lastRefresh >= 1000)
    {
        lastRefresh = millis();

        Serial1.printf("%s/\n",REFRESH_VALUES);
    }

    if(millis() - lastUpdate >= 60000)
    {
        lastUpdate = millis();

        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_UP,dpad_up);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_DOWN,dpad_down);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_LEFT,dpad_left);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_DPAD_RIGHT,dpad_right);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_ELEC_REQ,electromagnet_req);
        
        if(selected_leds_req==3)
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req+1);
        else
            Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,selected_leds_req);

        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_REM_REQ,remote_req);

        if(timeout_connection != -1)
            Serial1.printf("%s/%.2f\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,(timeout_connection * 150.0) / 1000.0);
        else
            Serial1.printf("%s/%.2f\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,0.0);

        Serial1.printf("%s/%d\n",KEYEXPR_CAR_MOVEMENT,car_movement);
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_CONTROLLER_STATUS,controller_status);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_LOAD_REQ,load_req);
        Serial1.printf("%s/%d\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,unload_req);
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_FAULT,fault);

    }

    fault=0;
    while(Serial1.available()>0)
    {
        command=Serial1.readStringUntil('\n');
        separator_index=command.lastIndexOf('/');
        serial_name=command.substring(0,separator_index);
        serial_value=command.substring(separator_index+1);

        if ( serial_name == KEYEXPR_CRANE_ELECTROMAGNET )
        {
            crane_electromagnet = serial_value.toInt();
            Serial.printf("crane_electromagnet/%d\n",crane_electromagnet);
        }
        else if ( serial_name == KEYEXPR_CRANE_ALLOW_REMOTE )
        {
            crane_allow_remote = serial_value.toInt();
            Serial.printf("crane_allow_remote/%d\n",crane_allow_remote);
        }
        else if ( serial_name == KEYEXPR_CRANE_SPEED_MODE )
        {
            crane_speed_mode = serial_value.toInt();
            Serial.printf("crane_speed_mode/%d\n",crane_speed_mode);
        }
        else if ( serial_name == KEYEXPR_CRANE_MH_CONTINUOUS )
        {
            crane_motor_horizontal_continuous = serial_value.toInt();
            Serial.printf("crane_motor_horizontal_continuous/%d\n",crane_motor_horizontal_continuous);
        }
        else if ( serial_name == KEYEXPR_CRANE_MV_CONTINUOUS )
        {
            crane_motor_vertical_continuous = serial_value.toInt();
            Serial.printf("crane_motor_vertical_continuous/%d\n",crane_motor_vertical_continuous);
        }
        else if ( serial_name == KEYEXPR_CRANE_FAULT )
        {
            crane_fault = serial_value.toInt();
            Serial.printf("crane_fault/%d\n",crane_fault);
        }
        else if ( serial_name == KEYEXPR_CRANE_PROCESS_STATUS )
        {
            crane_process_status = serial_value.toInt();
            Serial.printf("crane_process_status/%d\n",crane_process_status);
        }
        else if ( serial_name == KEYEXPR_CRANE_MH_LIMITS )
        {
            crane_motor_horizontal_limits = serial_value.toInt();
            Serial.printf("crane_motor_horizontal_limits/%d\n",crane_motor_horizontal_limits);
        }
        else if ( serial_name == KEYEXPR_CRANE_MV_LIMITS )
        {
            crane_motor_vertical_limits = serial_value.toInt();
            Serial.printf("crane_motor_vertical_limits/%d\n",crane_motor_vertical_limits);
        }
        else if ( serial_name == KEYEXPR_CRANE_CARGO )
        {
            crane_cargo = serial_value.toInt();
            Serial.printf("crane_cargo/%d\n",crane_cargo);
        }
        else if ( serial_name == TELEMATIC_ECU_ONLINE )
        {
            telematic_ecu_status=1;
            serial_fail_cnt=0;
            // Serial.printf("TELEMATIC_ECU_ONLINE/%d\n",telematic_ecu_status);
        }
        else if ( serial_name == MAIN_ECU_ONLINE )
        {
            Serial1.printf("%s/\n",MAIN_ECU_ONLINE);
        }
        else
        {
            Serial.printf("%s\n",serial_name);
            fault=1;
            
        }

    }

    if(prev_fault!=fault)
        Serial1.printf("%s/%d\n",KEYEXPR_CAR_FAULT,fault);


    prev_fault=fault;
    vTaskDelay(30 / portTICK_PERIOD_MS);
}
