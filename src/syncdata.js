JSRocket.SyncData = function () {

    "use strict";

    var _track = [];

    function getTrack(index) {
        return _track[index];
    }

    function getIndexForName(name) {
        for (var i = 0; i < _track.length; i++) {

            if (_track[i].name === name) {
                return i;
            }
        }

        return -1;
    }

    function getTrackLength() {
        return _track.length;
    }

    function createIndex(varName) {
        var track = new JSRocket.Track();
        track.name = varName;

        _track.push(track);
    }

    return {
        getTrack       :getTrack,
        getIndexForName:getIndexForName,
        getTrackLength :getTrackLength,
        createIndex    :createIndex
    };
};