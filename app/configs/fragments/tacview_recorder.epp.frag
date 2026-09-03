dataRecorder: ( ExposedDataRecorder
      eventName: "dashboard-@SCENARIO_ID@"
      enabledList: [ 43 42 ]
      outputHandler: ( RecorderOutputHandler
         components: {
            ( TacviewOutput
               port: 1236
               callsign: "poc-mixr/dashboard-@SCENARIO_ID@"
               fileName: "./app/data/recordings/mission-@SCENARIO_ID@.acmi"
               modelMap: { @MODEL_MAP@ }
               typeMap:  { @TYPE_MAP@ }
               colorMap: { @COLOR_MAP@ }
            )
         }
      )
   )