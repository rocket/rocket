JSRocket.SyncDeviceClient = function (cfg) {

    "use strict";

    var CMD_SET_KEY = 0,
        CMD_DELETE_KEY = 1,
        CMD_GET_TRACK = 2,
        CMD_SET_ROW = 3,
        CMD_PAUSE = 4,
        CMD_SAVE_TRACKS = 5;

    var _ws = new WebSocket(cfg.socketURL),
        _syncData = new JSRocket.SyncData(),
        _eventHandler = {
            'ready' :function () {
            },
            'update':function () {
            },
            'play'  :function () {
            },
            'pause' :function () {
            },
            'save' :function () {
            }
        };

    function onOpen() {

        _ws.binaryType = "arraybuffer";
        _ws.send('hello, synctracker!');
    }

    function onMessage(e) {

        var queue = (new Uint8Array(e.data)),
            cmd = queue[0],
            track, row, value, interpolation;

        //Handshake
        if (cmd === 104) {

            _eventHandler.ready();

            //PAUSE
        } else if (CMD_PAUSE === cmd) {

            if( queue[1] === 1) {
                _eventHandler.pause();
            } else {
                _eventHandler.play();
            }

            //SET_ROW
        } else if (CMD_SET_ROW === cmd) {

            row = toInt(queue.subarray(1, 5));

            _eventHandler.update(row);

            //SET_KEY
        } else if (CMD_SET_KEY === cmd) {

            track = toInt(queue.subarray(1, 5));
            row = toInt(queue.subarray(5, 9));

            //value = Math.round(toFloat(queue.subarray(9, 13)) * 100) / 100; //round to what's seen in Rocket tracks
            value = toFloat(queue.subarray(9, 13)); //use the values you see in Rocket statusbar

            interpolation = toInt(queue.subarray(13, 14));
            _syncData.getTrack(track).add(row, value, interpolation);

            //DELETE
        } else if (CMD_DELETE_KEY === cmd) {

            track = toInt(queue.subarray(1, 5));
            row = toInt(queue.subarray(5, 9));

            _syncData.getTrack(track).remove(row);

            //SAVE
        } else if (CMD_SAVE_TRACKS === cmd) {
            _eventHandler.save();
        }
    }

    function onClose(e) {
        console.warn(">> connection closed", e);
    }

    function onError(e) {
        console.error(">> connection error'd", e);
    }

    _ws.onopen = onOpen;
    _ws.onmessage = onMessage;
    _ws.onclose = onClose;
    _ws.onerror = onError;

    function getTrack(name) {

        var index = _syncData.getIndexForName(name);

        if (index > -1) {
            return _syncData.getTrack(index);
        }

        var utf8Name = encodeURIComponent(name).replace(/%([\dA-F]{2})/g, function(m, c) {
		return String.fromCharCode('0x' + c);
        });
        var message = [CMD_GET_TRACK,
                        (utf8Name.length >> 24) & 0xFF, (utf8Name.length >> 16) & 0xFF,
                        (utf8Name.length >> 8) & 0xFF, (utf8Name.length ) & 0xFF];

        for (var i = 0; i < utf8Name.length; i++) {
            message.push(utf8Name.charCodeAt(i));
        }

        _ws.send(new Uint8Array(message).buffer);

        _syncData.createIndex(name);
        return _syncData.getTrack(_syncData.getTrackLength() - 1);
    }

    function setRow(row) {

        var streamInt = [(row >> 24) & 0xFF,
                        (row >> 16) & 0xFF,
                        (row >> 8) & 0xFF,
                        (row      ) & 0xFF];

        _ws.send(new Uint8Array([CMD_SET_ROW, streamInt[0], streamInt[1], streamInt[2], streamInt[3]]).buffer);
    }

    function toInt(arr){

        var i = 0,
            view = new DataView(new ArrayBuffer(arr.length));

        for(;i < arr.length; i++) {
            view.setUint8(i, arr[i]);
        }

        if(view.byteLength === 1) {
            return view.getInt8(0);
        } else {
            return view.getInt32(0);
        }
    }

    function toFloat(arr) {
        var view = new DataView(new ArrayBuffer(4));
        view.setUint8(0, arr[0]);
        view.setUint8(1, arr[1]);
        view.setUint8(2, arr[2]);
        view.setUint8(3, arr[3]);

        return view.getFloat32(0);
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
