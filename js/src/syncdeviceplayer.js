JSRocket.SyncDevicePlayer = function (cfg) {

    "use strict";

    var _urlRequest,
        _syncData = new JSRocket.SyncData(),
        _eventHandler = {
            'ready':function () {
            },
            'error':function () {
            }
        };

    function load(url) {

        _urlRequest = new XMLHttpRequest();

        if (_urlRequest === null) {
            _eventHandler.error();
            return;
        }

        _urlRequest.open('GET', url, true);
        _urlRequest.onreadystatechange = urlRequestHandler;

        _urlRequest.send();
    }

    function urlRequestHandler() {

        if (_urlRequest.readyState === 4) {
            if (_urlRequest.status < 300) {
                readXML(_urlRequest.responseText);
            } else {
                _eventHandler.error();
            }
        }
    }

    function readXML(xmlString) {
        var key,
            t = 0, tLen, k = 0, kLen,
            xml = (new DOMParser()).parseFromString(xmlString, 'text/xml'),
            tracks = xml.getElementsByTagName('tracks');

        //<tracks>
        var trackList = tracks[0].getElementsByTagName('track');

        for (t, tLen = trackList.length; t < tLen; t++) {

            var track = getTrack(trackList[t].getAttribute('name')),
                keyList = trackList[t].getElementsByTagName('key');

            for (k = 0, kLen = keyList.length; k < kLen; k++) {
                key = keyList[k];
                track.add(parseInt(key.getAttribute('row'), 10),
                    parseFloat(key.getAttribute('value')),
                    parseInt(key.getAttribute('interpolation'), 10));

            }
        }

        _eventHandler.ready();
    }

    function getTrack(name) {

        var index = _syncData.getIndexForName(name);

        if (index > -1) {
            return _syncData.getTrack(index);
        }

        _syncData.createIndex(name);
        return _syncData.getTrack(_syncData.getTrackLength() - 1);
    }

    function setEvent(evt, handler) {
        _eventHandler[evt] = handler;
    }

    function nop() {

    }

    if (cfg.rocketXML === "" || cfg.rocketXML === undefined || cfg.rocketXML === undefined) {
        throw("[jsRocket] rocketXML is not set, try _syncDevice.setConfig({'rocketXML':'url/To/RocketXML.rocket'})");
    } else {
        load(cfg.rocketXML);
    }

    return {
        load    :load,
        getTrack:getTrack,
        update  :nop,
        on      :setEvent
    };
};
