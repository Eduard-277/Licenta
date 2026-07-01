#include <Arduino.h>
#include <WiFi.h>
#include <zenoh-pico.h>
#include <queue>

std::queue<String> commands;


#define CLIENT_OR_PEER 0 // 0: Client mode; 1: Peer mode
#if CLIENT_OR_PEER == 0
#define MODE "client"
#define LOCATOR "tcp/192.168.1.130:7447" // If empty, it will scout
#elif CLIENT_OR_PEER == 1
#define MODE "peer"
#define LOCATOR "udp/224.0.0.225:7447#iface=en0"
#else
#error "Unknown Zenoh operation mode. Check CLIENT_OR_PEER value."
#endif

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

#define REFRESH_VALUES "refresh"
#define TELEMATIC_ECU_ONLINE "telematic_online"
#define MAIN_ECU_ONLINE "main_online"

#define VALUE ""

z_owned_session_t s;

z_owned_subscriber_t sub_crane_electromagnet;
z_owned_subscriber_t sub_crane_allow_remote;
z_owned_subscriber_t sub_crane_speed_mode;
z_owned_subscriber_t sub_crane_motor_horizontal_continuous;
z_owned_subscriber_t sub_crane_motor_vertical_continuous;
z_owned_subscriber_t sub_crane_fault;
z_owned_subscriber_t sub_crane_process_status;
z_owned_subscriber_t sub_crane_motor_horizontal_limits;
z_owned_subscriber_t sub_crane_motor_vertical_limits;
z_owned_subscriber_t sub_crane_cargo;

z_owned_publisher_t pub_controller_dpad_up;
z_owned_publisher_t pub_controller_dpad_down;
z_owned_publisher_t pub_controller_dpad_left;
z_owned_publisher_t pub_controller_dpad_right;
z_owned_publisher_t pub_controller_electromagnet_req;
z_owned_publisher_t pub_controller_unload_req;
z_owned_publisher_t pub_controller_load_req;
z_owned_publisher_t pub_controller_remote_req;
z_owned_publisher_t pub_car_movement;
z_owned_publisher_t pub_connection_req_timeout;
z_owned_publisher_t pub_controller_status;
z_owned_publisher_t pub_controller_speed_req;
z_owned_publisher_t pub_car_fault;

z_get_options_t opts;
z_owned_closure_reply_t callback_getter_crane_electromagnet;
z_owned_closure_reply_t callback_getter_crane_allow_remote;
z_owned_closure_reply_t callback_getter_crane_speed_mode;
z_owned_closure_reply_t callback_getter_crane_motor_horizontal_continuous;
z_owned_closure_reply_t callback_getter_crane_motor_vertical_continuous;
z_owned_closure_reply_t callback_getter_crane_fault;
z_owned_closure_reply_t callback_getter_crane_process_status;
z_owned_closure_reply_t callback_getter_crane_motor_horizontal_limits;
z_owned_closure_reply_t callback_getter_crane_motor_vertical_limits;
z_owned_closure_reply_t callback_getter_crane_cargo;

// z_owned_bytes_t payload;
z_view_keyexpr_t ke_masina;

#define RX1PIN D8
#define TX1PIN D9
#define AWAKEPIN D10


const char *ssid = "";
const char *password = "";
String hostname = "ESP32 Masina";

WiFiServer server(80);

IPAddress local_IP(192, 168, 1, 131);

IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(1, 1, 1, 1);

unsigned long lastTimeoutLoop = 0;
int main_ecu_status=-1,prev_main_ecu_status=-1;

String command;
String serial_name;
String serial_value;
int separator_index=0;

int crane_cargo=0;
int crane_motor_horizontal_limits = 0;
int crane_motor_vertical_limits = 0;
int crane_electromagnet = 0;
int crane_allow_remote = 0;
int crane_speed_mode = 0;
int crane_motor_horizontal_continuous = 0;
int crane_motor_vertical_continuous = 0;
int crane_fault = 0;
int crane_process_status = 0;

int electromagnet_req = 0;
int unload_req = 0;
int load_req = 0;
int remote_req = 0;

int selected_leds = 0, prev_selected_leds = 0;


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


unsigned long previousMillis = 0;
unsigned long interval = 30000;


void initWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());
}

void data_handler(z_loaned_sample_t *sample, void *arg)
{
    z_view_string_t keystr;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
    z_owned_string_t value;
    z_bytes_to_string(z_sample_payload(sample), &value);

    const char *data = z_string_data(z_string_loan(&value));
    const char *topic = z_string_data(z_view_string_loan(&keystr));
    size_t length = z_string_len(z_view_string_loan(&keystr));

    // Serial.print(" >> [Subscription listener] Received (");
    // Serial.write(z_string_data(z_view_string_loan(&keystr)), z_string_len(z_view_string_loan(&keystr)));
    // Serial.print(", ");
    // Serial.write(z_string_data(z_string_loan(&value)), z_string_len(z_string_loan(&value)));
    // Serial.println(")");

    if (strncmp(topic, KEYEXPR_CRANE_ELECTROMAGNET, length) == 0 && length == strlen(KEYEXPR_CRANE_ELECTROMAGNET))
    {
        crane_electromagnet = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_ELECTROMAGNET,crane_electromagnet);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_ALLOW_REMOTE, length) == 0 && length == strlen(KEYEXPR_CRANE_ALLOW_REMOTE))
    {
        crane_allow_remote = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_ALLOW_REMOTE,crane_allow_remote);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_SPEED_MODE, length) == 0 && length == strlen(KEYEXPR_CRANE_SPEED_MODE))
    {
        crane_speed_mode = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_SPEED_MODE,crane_speed_mode);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_MH_CONTINUOUS, length) == 0 && length == strlen(KEYEXPR_CRANE_MH_CONTINUOUS))
    {
        crane_motor_horizontal_continuous = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MH_CONTINUOUS,crane_motor_horizontal_continuous);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_MV_CONTINUOUS, length) == 0 && length == strlen(KEYEXPR_CRANE_MV_CONTINUOUS))
    {
        crane_motor_vertical_continuous = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MV_CONTINUOUS,crane_motor_vertical_continuous);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_FAULT, length) == 0 && length == strlen(KEYEXPR_CRANE_FAULT))
    {
        crane_fault = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_FAULT,crane_fault);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_PROCESS_STATUS, length) == 0 && length == strlen(KEYEXPR_CRANE_PROCESS_STATUS))
    {
        crane_process_status = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_PROCESS_STATUS,crane_process_status);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_MH_LIMITS, length) == 0 && length == strlen(KEYEXPR_CRANE_MH_LIMITS))
    {
        crane_motor_horizontal_limits = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MH_LIMITS,crane_motor_horizontal_limits);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_MV_LIMITS, length) == 0 && length == strlen(KEYEXPR_CRANE_MV_LIMITS))
    {
        crane_motor_vertical_limits = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MV_LIMITS,crane_motor_vertical_limits);
    }
    else if (strncmp(topic, KEYEXPR_CRANE_CARGO, length) == 0 && length == strlen(KEYEXPR_CRANE_CARGO))
    {
        crane_cargo = atoi(data);
        Serial1.printf("%s/%d\n",KEYEXPR_CRANE_CARGO,crane_cargo);
    }

    z_string_drop(z_string_move(&value));
}

void reply_dropper(void *ctx)
{
    (void)(ctx);
    Serial.println(" >> Received query final notification");
}

void reply_handler(z_loaned_reply_t *oreply, void *ctx)
{
    (void)(ctx);
    if (z_reply_is_ok(oreply))
    {
        const z_loaned_sample_t *sample = z_reply_ok(oreply);
        z_view_string_t keystr;
        z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
        z_owned_string_t replystr;
        z_bytes_to_string(z_sample_payload(sample), &replystr);

        const char *data = z_string_data(z_string_loan(&replystr));
        const char *topic = z_string_data(z_view_string_loan(&keystr));
        size_t length = z_string_len(z_view_string_loan(&keystr));

        // Serial.print(" >> [Get listener] Received (");
        // Serial.write(z_string_data(z_view_string_loan(&keystr)), z_string_len(z_view_string_loan(&keystr)));
        // Serial.print(", ");
        // Serial.write(z_string_data(z_string_loan(&replystr)), z_string_len(z_string_loan(&replystr)));
        // Serial.println(")");

        if (strncmp(topic, KEYEXPR_CRANE_ELECTROMAGNET, length) == 0 && length == strlen(KEYEXPR_CRANE_ELECTROMAGNET))
        {
            crane_electromagnet = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_ELECTROMAGNET,crane_electromagnet);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_ALLOW_REMOTE, length) == 0 && length == strlen(KEYEXPR_CRANE_ALLOW_REMOTE))
        {
            crane_allow_remote = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_ALLOW_REMOTE,crane_allow_remote);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_SPEED_MODE, length) == 0 && length == strlen(KEYEXPR_CRANE_SPEED_MODE))
        {
            crane_speed_mode = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_SPEED_MODE,crane_speed_mode);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_MH_CONTINUOUS, length) == 0 && length == strlen(KEYEXPR_CRANE_MH_CONTINUOUS))
        {
            crane_motor_horizontal_continuous = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MH_CONTINUOUS,crane_motor_horizontal_continuous);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_MV_CONTINUOUS, length) == 0 && length == strlen(KEYEXPR_CRANE_MV_CONTINUOUS))
        {
            crane_motor_vertical_continuous = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MV_CONTINUOUS,crane_motor_vertical_continuous);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_FAULT, length) == 0 && length == strlen(KEYEXPR_CRANE_FAULT))
        {
            crane_fault = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_FAULT,crane_fault);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_PROCESS_STATUS, length) == 0 && length == strlen(KEYEXPR_CRANE_PROCESS_STATUS))
        {
            crane_process_status = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_PROCESS_STATUS,crane_process_status);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_MH_LIMITS, length) == 0 && length == strlen(KEYEXPR_CRANE_MH_LIMITS))
        {
            crane_motor_horizontal_limits = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MH_LIMITS,crane_motor_horizontal_limits);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_MV_LIMITS, length) == 0 && length == strlen(KEYEXPR_CRANE_MV_LIMITS))
        {
            crane_motor_vertical_limits = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_MV_LIMITS,crane_motor_vertical_limits);
        }
        else if (strncmp(topic, KEYEXPR_CRANE_CARGO, length) == 0 && length == strlen(KEYEXPR_CRANE_CARGO))
        {
            crane_cargo = atoi(data);
            Serial1.printf("%s/%d\n",KEYEXPR_CRANE_CARGO,crane_cargo);
        }

        z_string_drop(z_string_move(&replystr));
    }
    else
    {
        Serial.println(" >> Received an error");
    }
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("Connected to AP successfully!");
    Serial.print("RRSI: ");
    Serial.println(WiFi.RSSI());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("Disconnected from WiFi access point");
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    Serial.println("Trying to Reconnect");
    WiFi.begin(ssid, password);
}

bool session_open=0;

void setup() {

    pinMode(AWAKEPIN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
 // internet
    Serial.begin(115200);
    Serial1.begin(115200,SERIAL_8N1,RX1PIN,TX1PIN);

    // while (!Serial) {
    //   delay(10); 
    // }
    // Serial.println("\n\n--- SERIAL CONNECTED ---");

    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
        Serial.println("STA Failed to configure");
    }
    WiFi.setHostname(hostname.c_str());
    initWiFi();

    delay(2000);

    z_owned_config_t config;
    z_config_default(&config);
    zp_config_insert(z_config_loan_mut(&config), Z_CONFIG_MODE_KEY, MODE);
    if (strcmp(LOCATOR, "") != 0)
    {
        if (strcmp(MODE, "client") == 0)
        {
            zp_config_insert(z_config_loan_mut(&config), Z_CONFIG_CONNECT_KEY, LOCATOR);
        }
        else
        {
            zp_config_insert(z_config_loan_mut(&config), Z_CONFIG_LISTEN_KEY, LOCATOR);
        }
    }

    while(session_open==0)
    {
        Serial.print("Opening Zenoh Session...");
        delay(2000);

        if (z_open(&s, z_config_move(&config), NULL) < 0)
        {
            Serial.println("Unable to open session!");
        }
        else
        {
            Serial.println("OK");
            Serial.println("Zenoh setup finished!");
            session_open=1;
        }
    }

    Serial.println("Setting subscribers...");
    delay(2000);

    ///////SUBSCRIBERS

    z_owned_closure_sample_t callback_crane_electromagnet;
    z_closure_sample(&callback_crane_electromagnet, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ELECTROMAGNET);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_electromagnet, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_electromagnet),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_electromagnet subscriber.");
    }
    else
        Serial.println("Subscriber crane_electromagnet declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_allow_remote;
    z_closure_sample(&callback_crane_allow_remote, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ALLOW_REMOTE);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_allow_remote, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_allow_remote),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_allow_remote subscriber.");
    }
    else
        Serial.println("Subscriber crane_allow_remote declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_speed_mode;
    z_closure_sample(&callback_crane_speed_mode, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_SPEED_MODE);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_speed_mode, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_speed_mode),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_speed_mode subscriber.");
    }
    else
        Serial.println("Subscriber crane_speed_mode declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_motor_horizontal_continous;
    z_closure_sample(&callback_crane_motor_horizontal_continous, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_CONTINUOUS);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_motor_horizontal_continuous, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_motor_horizontal_continous),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_motor_horizontal_continuous subscriber.");
    }
    else
        Serial.println("Subscriber crane_motor_horizontal_continuous declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_motor_vertical_continuous;
    z_closure_sample(&callback_crane_motor_vertical_continuous, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_CONTINUOUS);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_motor_vertical_continuous, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_motor_vertical_continuous),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_motor_vertical_continuous subscriber.");
    }
    else
        Serial.println("Subscriber crane_motor_vertical_continuous declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_fault;
    z_closure_sample(&callback_crane_fault, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_FAULT);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_fault, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_fault),
                             NULL) < 0)
    {
        Serial.println("Unable to declare crane_fault subscriber.");
    }
    else
        Serial.println("Subscriber crane_fault declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_process_status;
    z_closure_sample(&callback_crane_process_status, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_PROCESS_STATUS);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_process_status, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_process_status),
                                NULL) < 0)
    {
        Serial.println("Unable to declare CRANE_PROCESS_STATUS subscriber.");
    }
    else
        Serial.println("Subscriber CRANE_PROCESS_STATUS declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_motor_horizontal_limits;
    z_closure_sample(&callback_crane_motor_horizontal_limits, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_LIMITS);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_motor_horizontal_limits, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_motor_horizontal_limits),
                                NULL) < 0)
    {
        Serial.println("Unable to declare crane_motor_horizontal_limits subscriber.");
    }
    else
        Serial.println("Subscriber crane_motor_horizontal_limits declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_motor_vertical_limits;
    z_closure_sample(&callback_crane_motor_vertical_limits, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_LIMITS);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_motor_vertical_limits, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_motor_vertical_limits),
                                NULL) < 0)
    {
        Serial.println("Unable to declare crane_motor_vertical_limits subscriber.");
    }
    else
        Serial.println("Subscriber crane_motor_vertical_limits declared succesfully");

    ////////////////////////////////////////////////////////

    z_owned_closure_sample_t callback_crane_cargo;
    z_closure_sample(&callback_crane_cargo, data_handler, NULL, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_CARGO);
    if (z_declare_subscriber(z_session_loan(&s), &sub_crane_cargo, z_view_keyexpr_loan(&ke_masina), z_closure_sample_move(&callback_crane_cargo),
                                NULL) < 0)
    {
        Serial.println("Unable to declare CRANE_CARGO subscriber.");
    }
    else
        Serial.println("Subscriber CRANE_CARGO declared succesfully");

    ///////GETTERS

    Serial.print("Setting getters...");
    delay(2000);

    // Declare Zenoh getter
    
    z_get_options_default(&opts);
    // Value encoding
    z_owned_bytes_t payload;
    if (strcmp(VALUE, "") != 0)
    {
        z_bytes_from_static_str(&payload, VALUE);
        opts.payload = z_bytes_move(&payload);
    }

    z_closure_reply(&callback_getter_crane_electromagnet, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ELECTROMAGNET);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_electromagnet), &opts) < 0)
    {
        Serial.println("Unable to send query crane_electromagnet.");
    }
    else
        Serial.println("Query crane_electromagnet sent succesfully");

    /////////////////////////////////////////////////////////

    
    z_closure_reply(&callback_getter_crane_allow_remote, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ALLOW_REMOTE);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_allow_remote), &opts) < 0)
    {
        Serial.println("Unable to send query crane_allow_remote.");
    }
    else
        Serial.println("Query crane_allow_remote sent succesfully");

    /////////////////////////////////////////////////////////

    z_closure_reply(&callback_getter_crane_speed_mode, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_SPEED_MODE);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_speed_mode), &opts) < 0)
    {
        Serial.println("Unable to send query crane_speed_mode.");
    }
    else
        Serial.println("Query crane_speed_mode sent succesfully");

    /////////////////////////////////////////////////////////

    
    z_closure_reply(&callback_getter_crane_motor_horizontal_continuous, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_CONTINUOUS);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_horizontal_continuous), &opts) < 0)
    {
        Serial.println("Unable to send query crane_motor_horizontal_continuous.");
    }
    else
        Serial.println("Query crane_motor_horizontal_continuous sent succesfully");

    /////////////////////////////////////////////////////////

    
    z_closure_reply(&callback_getter_crane_motor_vertical_continuous, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_CONTINUOUS);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_vertical_continuous), &opts) < 0)
    {
        Serial.println("Unable to send query crane_motor_vertical_continuous.");
    }
    else
        Serial.println("Query crane_motor_vertical_continuous sent succesfully");

    /////////////////////////////////////////////////////////

    
    z_closure_reply(&callback_getter_crane_fault, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_FAULT);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_fault), &opts) < 0)
    {
        Serial.println("Unable to send query crane_fault.");
    }
    else
        Serial.println("Query crane_fault sent succesfully");

    /////////////////////////////////////////////////////////

    
    z_closure_reply(&callback_getter_crane_process_status, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_PROCESS_STATUS);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_process_status), &opts) < 0)
    {
        Serial.println("Unable to send query CRANE_PROCESS_STATUS.");
    }
    else
        Serial.println("Query CRANE_PROCESS_STATUS sent succesfully");

    /////////////////////////////////////////////////////////


    z_closure_reply(&callback_getter_crane_motor_horizontal_limits, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_LIMITS);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_horizontal_limits), &opts) < 0)
    {
        Serial.println("Unable to send query CRANE_MH_LIMITS.");
    }
    else
        Serial.println("Query CRANE_MH_LIMITS sent succesfully");

    /////////////////////////////////////////////////////////


    z_closure_reply(&callback_getter_crane_motor_vertical_limits, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_LIMITS);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_vertical_limits), &opts) < 0)
    {
        Serial.println("Unable to send query CRANE_MV_LIMITS.");
    }
    else
        Serial.println("Query CRANE_MV_LIMITS sent succesfully");


    /////////////////////////////////////////////////////////


    z_closure_reply(&callback_getter_crane_cargo, reply_handler, reply_dropper, NULL);

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_CARGO);
    if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_cargo), &opts) < 0)
    {
        Serial.println("Unable to send query CRANE_CARGO.");
    }
    else
        Serial.println("Query CRANE_CARGO sent succesfully");

    ///////PUBLISHERS

    Serial.print("Setting publishers...");
    delay(2000);

    // Declare Zenoh publisher DPAD_UP
    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_DPAD_UP);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_dpad_up, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher DPAD_UP for key expression!");
    }
    else
        Serial.println("Publisher DPAD_UP declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_DPAD_DOWN);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_dpad_down, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher DPAD_DOWN for key expression!");
    }
    else
        Serial.println("Publisher DPAD_DOWN declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_DPAD_LEFT);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_dpad_left, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher DPAD_LEFT for key expression!");
    }
    else
        Serial.println("Publisher DPAD_LEFT declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_DPAD_RIGHT);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_dpad_right, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher DPAD_RIGHT for key expression!");
    }
    else
        Serial.println("Publisher DPAD_RIGHT declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_ELEC_REQ);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_electromagnet_req, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher electromagnet_req for key expression!");
    }
    else
        Serial.println("Publisher electromagnet_req declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_UNLOAD_REQ);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_unload_req, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher UNLOAD_REQ for key expression!");
    }
    else
        Serial.println("Publisher UNLOAD_REQ declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_LOAD_REQ);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_load_req, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher LOAD_REQ for key expression!");
    }
    else
        Serial.println("Publisher LOAD_REQ declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CONTROLLER_REM_REQ);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_remote_req, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher REM_REQ for key expression!");
    }
    else
        Serial.println("Publisher REM_REQ declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CAR_MOVEMENT);
    if (z_declare_publisher(z_session_loan(&s), &pub_car_movement, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher CAR_MOVEMENT for key expression!");
    }
    else
        Serial.println("Publisher CAR_MOVEMENT declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT);
    if (z_declare_publisher(z_session_loan(&s), &pub_connection_req_timeout, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher CAR_CONNECTION_REQ_TIMEOUT for key expression!");
    }
    else
        Serial.println("Publisher CAR_CONNECTION_REQ_TIMEOUT declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CAR_CONTROLLER_STATUS);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_status, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher CAR_CONTROLLER_STATUS for key expression!");
    }
    else
        Serial.println("Publisher CAR_CONTROLLER_STATUS declared succesfully");


    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CAR_CONTROLLER_SPEED_REQ);
    if (z_declare_publisher(z_session_loan(&s), &pub_controller_speed_req, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher CAR_CONTROLLER_SPEED_REQ for key expression!");
    }
    else
        Serial.println("Publisher CAR_CONTROLLER_SPEED_REQ declared succesfully");

    //////////////////////////////////////////

    z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CAR_FAULT);
    if (z_declare_publisher(z_session_loan(&s), &pub_car_fault, z_view_keyexpr_loan(&ke_masina), NULL) < 0)
    {
        Serial.println("Unable to declare publisher CAR_FAULT for key expression!");
    }
    else
        Serial.println("Publisher CAR_FAULT declared succesfully");

    // delay(2000);

    // if (zp_start_read_task(z_session_loan_mut(&s), NULL) < 0)
    //     Serial.println("Unable to start Zenoh read task!");
    // else
    //     Serial.println("Zenoh read task started");

    digitalWrite(AWAKEPIN,0);

    Serial1.printf("%s/\n",MAIN_ECU_ONLINE);
    lastTimeoutLoop = millis();
}

int rc=0,retry_cnt=3;
int prev_fault=-1,fault=-1;

void loop()
{
    if (millis() - lastTimeoutLoop >= 3000)
    {
        lastTimeoutLoop = millis();
        if(main_ecu_status==0 && main_ecu_status!=prev_main_ecu_status)
        {
            
            fault=2;
            Serial.printf("%s/%d\n",KEYEXPR_CAR_FAULT,fault);
        }
        else if(main_ecu_status==1 && main_ecu_status!=prev_main_ecu_status)
        {
            
            fault=0;
            Serial.printf("%s/%d\n",KEYEXPR_CAR_FAULT,fault);
            
        }
        prev_main_ecu_status=main_ecu_status;
        main_ecu_status=0;
        Serial1.printf("%s/\n",MAIN_ECU_ONLINE);
    }

    while(Serial1.available()>0)
    {
        String aux_command=Serial1.readStringUntil('\n');
        if(commands.size()>30)
            commands.pop();
        commands.push(aux_command);
    }

    if(WiFi.status() == WL_CONNECTED)
    while(!commands.empty())
    {
        command=commands.front();
        commands.pop();

        separator_index=command.lastIndexOf('/');
        serial_name=command.substring(0,separator_index);
        serial_value=command.substring(separator_index+1);

        retry_cnt=3;
        rc=0;
        do{
            if(retry_cnt<3)
                delay(3000);
        
            if ( serial_name == KEYEXPR_CONTROLLER_DPAD_UP )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_dpad_up), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing dpad_up data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_DPAD_UP,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_DPAD_DOWN )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_dpad_down), z_bytes_move(&payload), NULL);
                if ( rc < 0)
                    Serial.println("Error while publishing dpad_down data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_DPAD_DOWN,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_DPAD_LEFT )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc =z_publisher_put(z_publisher_loan(&pub_controller_dpad_left), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing dpad_left data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_DPAD_LEFT,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_DPAD_RIGHT )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_dpad_right), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing dpad_right data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_DPAD_RIGHT,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_REM_REQ )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_remote_req), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing rem_req data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_REM_REQ,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                int rc=z_publisher_put(z_publisher_loan(&pub_connection_req_timeout), z_bytes_move(&payload), NULL);
                if ( rc < 0)
                    Serial.printf("Error while publishing connection_req_timeout data %d\n",rc);
                Serial.printf("%s/%s\n",KEYEXPR_CAR_CONNECTION_REQ_TIMEOUT,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_ELEC_REQ )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_electromagnet_req), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CONTROLLER_ELEC_REQ data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_ELEC_REQ,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_UNLOAD_REQ )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_unload_req), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CONTROLLER_UNLOAD_REQ data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_UNLOAD_REQ,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CONTROLLER_LOAD_REQ )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_load_req), z_bytes_move(&payload), NULL);
                if ( rc< 0)
                    Serial.println("Error while publishing CONTROLLER_LOAD_REQ data");
                Serial.printf("%s/%s\n",KEYEXPR_CONTROLLER_LOAD_REQ,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CAR_MOVEMENT )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_car_movement), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CAR_MOVEMENT data");
                Serial.printf("%s/%s\n",KEYEXPR_CAR_MOVEMENT,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CAR_CONTROLLER_STATUS )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_status), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CAR_CONTROLLER_STATUS data");
                Serial.printf("%s/%s\n",KEYEXPR_CAR_CONTROLLER_STATUS,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CAR_CONTROLLER_SPEED_REQ )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                rc=z_publisher_put(z_publisher_loan(&pub_controller_speed_req), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CAR_CONTROLLER_SPEED_REQ data");
                Serial.printf("%s/%s\n",KEYEXPR_CAR_CONTROLLER_SPEED_REQ,serial_value.c_str());
            }
            else if ( serial_name == KEYEXPR_CAR_FAULT )
            {
                z_owned_bytes_t payload;
                z_bytes_copy_from_str(&payload, serial_value.c_str());

                if(fault!=2)
                {
                    if(serial_value == "0")
                        fault=0;
                    else if(serial_value == "1")
                        fault=1;
                }

                rc=z_publisher_put(z_publisher_loan(&pub_car_fault), z_bytes_move(&payload), NULL);
                if (rc < 0)
                    Serial.println("Error while publishing CAR_FAULT data");
                Serial.printf("%s/%s\n",KEYEXPR_CAR_FAULT,serial_value.c_str());
            }
            else if ( serial_name == TELEMATIC_ECU_ONLINE )
            {
                Serial1.printf("%s/\n",TELEMATIC_ECU_ONLINE);
            }
            else if ( serial_name == MAIN_ECU_ONLINE )
            {
                main_ecu_status=1;
            }
            else if ( serial_name == REFRESH_VALUES )
            {

                Serial.printf("%s/%s\n",REFRESH_VALUES,serial_value.c_str());

                z_closure_reply(&callback_getter_crane_electromagnet, reply_handler, reply_dropper, NULL);

                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ELECTROMAGNET);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_electromagnet), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_electromagnet.");
                }
                else
                    Serial.println("Query crane_electromagnet sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                
                z_closure_reply(&callback_getter_crane_allow_remote, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_ALLOW_REMOTE);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_allow_remote), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_allow_remote.");
                }
                else
                    Serial.println("Query crane_allow_remote sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                z_closure_reply(&callback_getter_crane_speed_mode, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_SPEED_MODE);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_speed_mode), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_speed_mode.");
                }
                else
                    Serial.println("Query crane_speed_mode sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                
                z_closure_reply(&callback_getter_crane_motor_horizontal_continuous, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_CONTINUOUS);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_horizontal_continuous), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_motor_horizontal_continuous.");
                }
                else
                    Serial.println("Query crane_motor_horizontal_continuous sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                
                z_closure_reply(&callback_getter_crane_motor_vertical_continuous, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_CONTINUOUS);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_vertical_continuous), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_motor_vertical_continuous.");
                }
                else
                    Serial.println("Query crane_motor_vertical_continuous sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                
                z_closure_reply(&callback_getter_crane_fault, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_FAULT);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_fault), &opts) < 0)
                {
                    Serial.println("Unable to send query crane_fault.");
                }
                else
                    Serial.println("Query crane_fault sent succesfully");
            
                /////////////////////////////////////////////////////////
            
                
                z_closure_reply(&callback_getter_crane_process_status, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_PROCESS_STATUS);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_process_status), &opts) < 0)
                {
                    Serial.println("Unable to send query CRANE_PROCESS_STATUS.");
                }
                else
                    Serial.println("Query CRANE_PROCESS_STATUS sent succesfully");

                /////////////////////////////////////////////////////////


                z_closure_reply(&callback_getter_crane_motor_horizontal_limits, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MH_LIMITS);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_horizontal_limits), &opts) < 0)
                {
                    Serial.println("Unable to send query CRANE_MH_LIMITS.");
                }
                else
                    Serial.println("Query CRANE_MH_LIMITS sent succesfully");
            
                /////////////////////////////////////////////////////////
            
            
                z_closure_reply(&callback_getter_crane_motor_vertical_limits, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_MV_LIMITS);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_motor_vertical_limits), &opts) < 0)
                {
                    Serial.println("Unable to send query CRANE_MV_LIMITS.");
                }
                else
                    Serial.println("Query CRANE_MV_LIMITS sent succesfully");
            
            
                /////////////////////////////////////////////////////////
            
            
                z_closure_reply(&callback_getter_crane_cargo, reply_handler, reply_dropper, NULL);
            
                z_view_keyexpr_from_str_unchecked(&ke_masina, KEYEXPR_CRANE_CARGO);
                if (z_get(z_session_loan(&s), z_view_keyexpr_loan(&ke_masina), "", z_closure_reply_move(&callback_getter_crane_cargo), &opts) < 0)
                {
                    Serial.println("Unable to send query CRANE_CARGO.");
                }
                else
                    Serial.println("Query CRANE_CARGO sent succesfully");


            }
                
                
            --retry_cnt;
        }while(rc<0 && retry_cnt!=0);

    }

    if(prev_fault!=fault)
    {
        z_owned_bytes_t payload;

        switch(fault)
        {
            case 0:
            digitalWrite(LED_GREEN,LOW);
            digitalWrite(LED_RED,HIGH);
            digitalWrite(LED_BLUE,HIGH);

            
            z_bytes_copy_from_str(&payload, "0");

            if (z_publisher_put(z_publisher_loan(&pub_car_fault), z_bytes_move(&payload), NULL) < 0)
                Serial.println("Error while publishing CAR_FAULT data");

            break;

            case 1:
            digitalWrite(LED_BLUE,LOW);
            digitalWrite(LED_GREEN,HIGH);
            digitalWrite(LED_RED,HIGH);

            z_bytes_copy_from_str(&payload, "1");

            if (z_publisher_put(z_publisher_loan(&pub_car_fault), z_bytes_move(&payload), NULL) < 0)
                Serial.println("Error while publishing CAR_FAULT data");

            break;

            case 2:
            digitalWrite(LED_RED,LOW);
            digitalWrite(LED_GREEN,HIGH);
            digitalWrite(LED_BLUE,HIGH);

            z_bytes_copy_from_str(&payload, "2");

            if (z_publisher_put(z_publisher_loan(&pub_car_fault), z_bytes_move(&payload), NULL) < 0)
                Serial.println("Error while publishing CAR_FAULT data");

            break;
        }
    }

    prev_fault=fault;

}

