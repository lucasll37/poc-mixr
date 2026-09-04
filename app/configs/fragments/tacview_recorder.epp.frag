dataRecorder: ( ExposedDataRecorder
      eventName: "dashboard-@SCENARIO_ID@"
      enabledList: [ 43 42 ]
      outputHandler: ( RecorderOutputHandler
         components: {
            ( TacviewOutput
               // 1236 -- a porta PROPRIA do ./app, como app/README.md sempre
               // documentou ("diferente de single-thread/multi-thread (1234) e
               // bandit (1235), de proposito, pra rodar junto sem colidir").
               // Estava 1234 aqui, ou seja o app disputava a porta com as pocs
               // -- inofensivo enquanto cada um rodava sozinho, e nao mais:
               // com o ./app rodando TAMBEM os cenarios das pocs, dois testes
               // de saida seguidos (patrol e multi-thread) passaram a usar a
               // mesma porta e um deles morreu com SIGSEGV, uma vez em duas
               // execucoes da suite.
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