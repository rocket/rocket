JSRocket.SyncDevice = function () {

    "use strict";

    var _connected = false,
        _device,
        _previousIntRow,
        _config = {
            "socketURL":"ws://localhost:1339",
            "rocketXML":""
        },
        _eventHandler = {
            'ready' :function () {
            },
            'update':function () {
            },
            'play'  :function () {
            },
            'pause' :function () {
            }
        };

    function init(mode) {
        if (mode === "demo") {
            _device = new JSRocket.SyncDevicePlayer(_config);
        } else {
            _device = new JSRocket.SyncDeviceClient(_config);
        }

        _device.on('ready', deviceReady);
        _device.on('update', deviceUpdate);
        _device.on('play', devicePlay);
        _device.on('pause', devicePause);
    }

    function getConfig() {
        return _config;
    }

    function setConfig(cfg) {
        for (var option in cfg) {
            if (cfg.hasOwnProperty(option)) {
                _config[option] = cfg[option];
            }
        }

        return _config;
    }

    function deviceReady() {
        _connected = true;
        _eventHandler.ready();
    }

    function deviceUpdate(row) {
        _eventHandler.update(row);
    }

    function devicePlay() {
        _eventHandler.play();
    }

    function devicePause() {
        _eventHandler.pause();
    }

    function getTrack(name) {
        if (_connected) {
            return _device.getTrack(name);
        } else {
            return null;
        }
    }

    function update(row) {
        //no need to update rocket on float rows
        if (Math.floor(row) !== _previousIntRow) {
            _previousIntRow = Math.floor(row);
            _device.update(_previousIntRow);
        }
    }

    function setEvent(evt, handler) {
        _eventHandler[evt] = handler;
    }

    return {
        init     :init,
        setConfig:setConfig,
        getConfig:getConfig,
        getTrack :getTrack,
        update   :update,
        on       :setEvent
    };
};
