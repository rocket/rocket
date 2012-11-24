JSRocket.Track = function () {

    "use strict";

    var STEP = 0,
        LINEAR = 1,
        SMOOTH = 2,
        RAMP = 3;

    var _track = [],
        _index = [];

    function getValue(row) {
        var intRow = Math.floor(row),
            bound = getBound(intRow),
            lower = bound.low,
            upper = bound.high,
            v;

        //console.log("lower:", lower, " upper:", upper, _track, _track[lower]);

        if (isNaN(lower)) {

            return NaN;

        } else if ((isNaN(upper)) || (_track[lower].interpolation === STEP)) {

            return _track[lower].value;

        } else {

            switch (_track[lower].interpolation) {

                case LINEAR:
                    v = (row - lower) / (upper - lower);
                    return _track[lower].value + (_track[upper].value - _track[lower].value) * v;

                case SMOOTH:
                    v = (row - lower) / (upper - lower);
                    v = v * v * (3 - 2 * v);
                    return (_track[upper].value * v) + (_track[lower].value * (1 - v));

                case RAMP:
                    v = Math.pow((row - lower) / (upper - lower), 2);
                    return _track[lower].value + (_track[upper].value - _track[lower].value) * v;
            }
        }

        return NaN;
    }

    function getBound(rowIndex) {
        var lower = NaN,
            upper = NaN;

        for (var i = 0; i < _index.length; i++) {

            if (_index[i] <= rowIndex) {

                lower = _index[i];

            } else if (_index[i] >= rowIndex) {

                upper = _index[i];
                break;
            }
        }

        return {"low":lower, "high":upper};
    }

    function add(row, value, interpolation) {
        remove(row);

        //index lookup table
        _index.push(row);
        _track[row] = { "value"         :value,
                        "interpolation" :interpolation};

        //lowest first
        _index = _index.sort(function (a, b) {
            return a - b;
        });
    }

    function remove(row) {
        if (_index.indexOf(row) > -1) {
            _index.splice(_index.indexOf(row), 1);
            delete _track[row];
        }
    }

    return {
        getValue:getValue,
        add     :add,
        remove  :remove
    };
};