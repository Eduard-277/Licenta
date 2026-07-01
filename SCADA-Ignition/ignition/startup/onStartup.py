def onStartup():
	startup_topics = [
	    "SCADA/crane/speed_mode",
		"SCADA/crane/electromagnet",
		"SCADA/crane/allow_remote",
		"SCADA/crane/control/up",
		"SCADA/crane/control/right",
		"SCADA/crane/control/left",
		"SCADA/crane/control/down",
		"SCADA/crane/motor_horizontal/power",
		"SCADA/crane/motor_vertical/power",
		"SCADA/crane/motor_horizontal/velocity",
		"SCADA/crane/motor_vertical/velocity",
		"SCADA/crane/motor_horizontal/specified/distance",
		"SCADA/crane/motor_vertical/specified/distance",
		"SCADA/crane/motor_horizontal/specified/movement",
		"SCADA/crane/motor_vertical/specified/movement",
		"SCADA/crane/motor_horizontal/continuous",
		"SCADA/crane/motor_vertical/continuous"
	]
	
	logger = system.util.getLogger("MQTT_Startup")
	
	for topic in startup_topics:
	    try:
	        system.cirruslink.engine.publish("Zenoh router", topic, "0".encode(), 0, True)
	    except Exception as e:
	        logger.error("Failed to initialize topic {}: {}".format(topic, str(e)))
	
	logger.info("MQTT Startup Initialization Complete.")