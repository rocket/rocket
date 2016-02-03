JSRocket.Track = function () {

    "use strict";

    var STEP = 0,
        LINEAR = 1,
        SMOOTH = 2,
        RAMP = 3;

    var data = [];

    function findKeyIndex(keys, row) {
        var lo = 0, hi = keys.length;
        while (lo < hi) {
            var mi = ((hi + lo) / 2) | 0;

            if (keys[mi] < row) {
                lo = mi + 1;
            } else if (keys[mi] > row) {
                hi = mi;
            } else {
                return mi;
            }
        }
        return lo - 1;
    }

    function getValue(row) {
        var keys = Object.keys(data);

        if (!keys.length) {
            return 0.0;
        }

        var idx = findKeyIndex(keys, Math.floor(row));
        if (idx < 0) {
            return data[keys[0]].value;
        }
        if (idx > keys.length - 2) {
            return data[keys[keys.length - 1]].value;
        }

        // lookup keys and values
        var k0 = keys[idx], k1 = keys[idx + 1];
        var a = data[k0].value;
        var b = data[k1].value;

        // interpolate
        var t = (row - k0) / (k1 - k0);
        switch (data[k0].interpolation) {
        case 0:
          return a;
        case 1:
          return a + (b - a) * t;
        case 2:
          return a + (b - a) * t * t * (3 - 2 * t);
        case 3:
          return a + (b - a) * Math.pow(t, 2.0);
        }
    }

    function add(row, value, interpolation) {
        data[row] = { "value"         :value,
                      "interpolation" :interpolation};
    }

    function remove(row) {
        delete data[row];
    }

    return {
        getValue:getValue,
        add     :add,
        remove  :remove
    };
};
