var OpenLocationCode = require("open-location-code").OpenLocationCode;
var olc = new OpenLocationCode();

//Max length of pluscode is 15 character.
//Max length of geocoding is (maybe?) 64 character.

var dict = {
    "Request":0,
    "PlusCode":"",
    "RGeoResult":"",
    "Accuracy":0
};

var pluscodedict = {
    "Request":0,
    "PlusCode":"",
    "Accuracy":0
};

var revgeodict = {
    "RGeoResult":""
};

var watchlang = "en_US";

function getrevgeo(lat, lon){
    var req = new XMLHttpRequest();
    req.open("GET", "https://nominatim.openstreetmap.org/reverse?format=json&zoom=10&" +
    "lat=" + lat + "&lon=" + lon, true);
    req.setRequestHeader("accept-language", watchlang);
    req.onload = function() {
        if (req.status === 200) {
            //console.log(req.responseText);
            var response = JSON.parse(req.responseText);
            var address = `${response.address.city}, ${response.address.province}, ${response.address.country}`;
            if (!response.error){
                revgeodict.RGeoResult = address;
                console.log("Address:", address);
            }
            else{
                revgeodict.RGeoResult = "";
            }
            updatewatch(revgeodict);
        }
    }
    req.send();
}

function updatewatch(d){
    console.log("Sending message:",JSON.stringify(d));
    Pebble.sendAppMessage(d,
        function() {
            console.log("AppMessage Success!");
        },
        function() {
            console.log("AppMessage Failed!");
        }
    )
}

function locationSuccess(pos){
    var coords = pos.coords;
    pluscodedict.Accuracy = coords.accuracy;
    pluscodedict.PlusCode = olc.encode(coords.latitude, coords.longitude);
    if (navigator.onLine){
        pluscodedict.Request = 1;
        getrevgeo(coords.latitude, coords.longitude);
    }
    else{
        pluscodedict.Request = 0; // End of communication
        console.log("Watch is offline!");
    }
    updatewatch(pluscodedict);
    console.log("Location:", coords.latitude, coords.longitude);
}

function locationError(err){
    updatewatch({"Request":2});
    console.log("Location error:", err.message);
}

function getcode(){
    console.log("Getting location..");
    window.navigator.geolocation.getCurrentPosition(
        locationSuccess, locationError, locationOptions
    );
}

var locationOptions = {
    "timeout": 5000,
    "maximumAge": 0,
    "enableHighAccuracy": true
}

Pebble.addEventListener("ready",
    function(e) {
        console.log("OpenLocationCode app started!");
        watchlang = Pebble.getActiveWatchInfo().language;
        console.log('Watch language is ', watchlang);
        getcode();
        //navigator.onLine = true; // for debug
        //locationSuccess({"coords":{"latitude":36.056813,"longitude":129.371699,"accuracy":99999}}); // for debug
    }
);

Pebble.addEventListener("appmessage",
    function(e) {
        var dict = e.payload;
        console.log('Got message:', JSON.stringify(dict));
        if (dict['Request'] == 0){
            getcode();
        }
    }
)
