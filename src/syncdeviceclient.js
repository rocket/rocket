JSRocket.SyncDeviceClient = function (cfg) {

    "use strict";

    var CMD_SET_KEY = 0,
        CMD_DELETE_KEY = 1,
        CMD_GET_TRACK = 2,
        CMD_SET_ROW = 3,
        CMD_PAUSE = 4,
        CMD_SAVE_TRACKS = 5;

    var _queueClearTimer,
        _currentCommand = -1,
        _queue = [],
        _ws = new Websock(),
        _syncData = new JSRocket.SyncData(),
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

    _ws.open(cfg.socketURL);

    function onOpen() {
        _ws.send_string('hello, synctracker!');
    }

    function onMessage() {

        var msg = _ws.rQshiftBytes();

        _queue = (_queue).concat(msg);

        readStream();
    }

    function readStream() {

        var len = _queue.length,
            track, row, value, interpolation;

        if (_currentCommand === -1) {
            _currentCommand = _queue[0];
        }

        //Handshake
        if ((_currentCommand === 104) && (len >= 12)) {

            _queue = [];
            _currentCommand = -1;

            _eventHandler.ready();

            //PAUSE
        } else if ((CMD_PAUSE === _currentCommand) && (len >= 2)) {

            value = parseInt(_queue[1], 10);

            _queue = _queue.slice(2);
            _currentCommand = -1;

            if (value === 1) {
                _eventHandler.pause();
            } else {
                _eventHandler.play();
            }

            //SET_ROW
        } else if ((CMD_SET_ROW === _currentCommand) && (len >= 5)) {

            row = toInt(_queue.slice(1, 5));

            _queue = _queue.slice(5);
            _currentCommand = -1;

            _eventHandler.update(row);

            //SET_KEY
        } else if ((CMD_SET_KEY === _currentCommand) && (len >= 14)) {

            track = toInt(_queue.slice(1, 5));
            row = toInt(_queue.slice(5, 9));
            value = parseInt(Math.round(toFloat(_queue.slice(9, 13)) * 1000) / 1000, 10);
            interpolation = parseInt(_queue.slice(13, 14).join(''), 10);

            _syncData.getTrack(track).add(row, value, interpolation);

            _queue = _queue.slice(14);
            _currentCommand = -1;

            //don't set row, as this could also be a interpolation change
            _eventHandler.update();

            //DELETE
        } else if ((CMD_DELETE_KEY === _currentCommand) && (len >= 9)) {

            track = toInt(_queue.slice(1, 5));
            row = toInt(_queue.slice(5, 9));

            _syncData.getTrack(track).remove(row);

            _queue = _queue.slice(9);
            _currentCommand = -1;

            _eventHandler.update(row);

            //SAVE
        } else if (CMD_SAVE_TRACKS === _currentCommand) {

            //console.log(">> TRACKS WERE SAVED");

            _queue = _queue.slice(1);
            _currentCommand = -1;
        }

        //clearing what's left in the queue
        clearInterval(_queueClearTimer);

        if (_queue.length >= 2) {
            _queueClearTimer = setInterval(readStream, 1);
        }
    }

    function onClose() {
        //console.log(">> connection closed");
    }

    function onError() {
        //console.error(">> connection error'd");
    }

    _ws.on('open', onOpen);
    _ws.on('message', onMessage);
    _ws.on('close', onClose);
    _ws.on('error', onError);

    function getTrack(name) {

        var index = _syncData.getIndexForName(name);

        if (index > -1) {
            return _syncData.getTrack(index);
        }

        _ws.send([CMD_GET_TRACK, 0, 0, 0, _syncData.getTrackLength(), 0, 0, 0, (name.length)]);
        _ws.send_string(name);
        _ws.flush();
        _syncData.createIndex(name);
        return _syncData.getTrack(_syncData.getTrackLength() - 1);
    }

    function setRow(row) {

        var streamInt = [(row >> 24) & 0xFF,
                        (row >> 16) & 0xFF,
                        (row >> 8) & 0xFF,
                        (row      ) & 0xFF];

        _ws.send([CMD_SET_ROW, streamInt[0], streamInt[1], streamInt[2], streamInt[3]]);
        _ws.flush();
    }

    function toInt(arr){
        var res = 0,
            i = arr.length - 1;

        for(; i > 0; i--) {
            res += parseInt(arr[i], 10) * Math.pow(256, (arr.length - 1) - i);
        }

        return res;
    }

    function toFloat(arr) {
        //identical to ws.rQshift32(), but no need to read the queue again
        var i = 0,
            n = (arr[i++] << 24) +
                (arr[i++] << 16) +
                (arr[i++] << 8) +
                (arr[i++]      ),
        //https://groups.google.com/forum/?fromgroups=#!topic/comp.lang.javascript/YzqYOCyWlNA
            sign = (n >> 31) * 2 + 1, // +1 or -1.
            exp = (n >>> 23) & 0xff,
            mantissa = n & 0x007fffff;

        if (exp === 0xff) {
            // NaN or Infinity
            return mantissa ? NaN : sign * Infinity;
        } else if (exp) {
            // Normalized value
            exp -= 127;

            // Add implicit bit in normal mode.
            mantissa |= 0x00800000;
        } else {
            // Subnormal number
            exp = -126;
        }
        return sign * mantissa * Math.pow(2, exp - 23);
    }

    function setEvent(evt, handler) {
        _eventHandler[evt] = handler;
    }

    return {
        getTrack:getTrack,
        update  :setRow,
        on      :setEvent
    };
};
