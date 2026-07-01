def onTagChange(initialChange, newValue, previousValue, event, executionCount):
	
	trigger_path = str(event.tagPath)
	
	mqtt_topic = ""
	
	topic_map = {
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/speed_mode": "SCADA/crane/speed_mode",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/electromagnet": "SCADA/crane/electromagnet",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/allow_remote": "SCADA/crane/allow_remote",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/control/up": "SCADA/crane/control/up",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/control/right": "SCADA/crane/control/right",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/control/left": "SCADA/crane/control/left",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/control/down": "SCADA/crane/control/down",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_horizontal/power": "SCADA/crane/motor_horizontal/power",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_vertical/power": "SCADA/crane/motor_vertical/power",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_horizontal/velocity": "SCADA/crane/motor_horizontal/velocity",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_vertical/velocity": "SCADA/crane/motor_vertical/velocity",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_horizontal/specified/distance": "SCADA/crane/motor_horizontal/specified/distance",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_vertical/specified/distance": "SCADA/crane/motor_vertical/specified/distance",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_horizontal/specified/movement": "SCADA/crane/motor_horizontal/specified/movement",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_vertical/specified/movement": "SCADA/crane/motor_vertical/specified/movement",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_horizontal/continuous": "SCADA/crane/motor_horizontal/continuous",
	"[MQTT Engine]Zenoh_router_tags/SCADA/crane/motor_vertical/continuous": "SCADA/crane/motor_vertical/continuous"}

	if trigger_path in topic_map:
		mqtt_topic = topic_map[trigger_path]
		system.cirruslink.engine.publish("Zenoh router", mqtt_topic, str(newValue.value).encode(), 0, 0)
	else:
		logger = system.util.getLogger("MQTTPublisher")
    	logger.warn("Tag triggered script but is not mapped: " + trigger_path)