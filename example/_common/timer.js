var Timer = (function () {

    "use strict";

    var RESOLUTION = 250,
        LENGTH = 32,
        _delta,
        _msTime = 0,
        _position,
        _timer,
        _eventList = {
            "progress":function () {},
            "ended"   :function () {}
        };

    function play() {
        clearInterval(_timer);
        _delta = Date.now();
        _timer = setInterval(progress, RESOLUTION);
    }

    function progress() {
        _msTime += Date.now() - _delta;

        _position = _msTime / 1000;

        _eventList.progress(_position);

        if (_position >= LENGTH) {
            onEnded();
        }

        _delta = Date.now();
    }

    function pause() {
        clearInterval(_timer);
    }

    function stop() {
        clearInterval(_timer);
        _msTime = 0;
    }

    function onEnded() {
        clearInterval(_timer);
        _eventList.ended();
    }

    function setResolution(newResolution) {

        if (!isNaN(newResolution) && newResolution >= 0) {
            RESOLUTION = newResolution;
        }
    }

    function setPosition(pos) {
        _msTime = pos * 1000;
    }

    function getPosition() {
        return _position;
    }

    function setEvent(name, fn) {

        _eventList[name] = fn;
    }

    function setLength(len) {
        LENGTH = len;
    }

    return {
        getPosition      :getPosition,
        setPosition      :setPosition,
        setResolutionInMs:setResolution,
        setLength        :setLength,
        play             :play,
        pause            :pause,
        stop             :stop,
        on               :setEvent
    };
});