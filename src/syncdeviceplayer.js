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
        var xml = (new DOMParser()).parseFromString(xmlString, 'text/xml'),
            tracks = xml.getElementsByTagName("tracks");

        //<tracks>
        for (var i = 0; i < tracks.length; i++) {
            //<tracks><track>
            var trackList = tracks[i].getElementsByTagName("track");

            for (var c = 0; c < trackList.length; c++) {

                var track = getTrack(trackList[c].getAttribute("name")),
                    keyList = trackList[c].getElementsByTagName("key");

                for (var u = 0; u < keyList.length; u++) {

                    track.add(parseInt(keyList[u].getAttribute("row"), 10),
                        parseFloat(keyList[u].getAttribute("value")),
                        parseInt(keyList[u].getAttribute("interpolation"), 10));
                }
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