//vat setting=[{name:overview, value:true},{name:events, value:true},{name:detspeed, value:true},{name:technicaldata, value:true},{name:drivercard, value:true},{name:run_web_app, value:true}]

function findItem(item) {
    for (let data of settingData) {
        if (data.name == item) {
            return data;
        }
    }
}
function findStatusItem(item) {
    for (let data of statusData) {
        if (data.name == item) {
            return data;
        }
    }
}

function populateStorageStatus()
{
    var stat_version = document.getElementById("stat_version");
    var stat_sd = document.getElementById("stat_sd");
    var stat_flash = document.getElementById("stat_flash");  
    stat_version.innerHTML = findStatusItem('version').val;
    stat_sd.innerHTML = findStatusItem('sd_storage').val;
    stat_flash.innerHTML = findStatusItem('flash_storage').val;
}

function removeElementsByLang() {
    var lang_index = findItem('web_lang').val;
    var elements;
    if (lang_index == 1) {

        elements = document.getElementsByClassName("cs");
    }
    else if (lang_index == 2) {
        elements = document.getElementsByClassName("en");
    }
    while (elements.length > 0) {
        elements[0].parentNode.removeChild(elements[0]);
    }
}

function populateSettings() {
    var overview = document.getElementById('overview');
    overview.checked = findItem('overview').val;

    var events = document.getElementById("events");
    events.checked = findItem('events').val;

    var detspeed = document.getElementById("detspeed");
    detspeed.checked = findItem('detspeed').val;

    var technicaldata = document.getElementById("technicaldata");
    technicaldata.checked = findItem('technicaldata').val;

    var drivercard = document.getElementById("drivercard");
    drivercard.checked = findItem('drivercard').val;

    var activity_from = document.getElementById("activity_from");
    activity_from.value = findItem('activity_from').val;

    var activity_to = document.getElementById("activity_to");
    activity_to.value = findItem('activity_to').val;

    var wifi_ssid = document.getElementById("ssid");
    wifi_ssid.value = findItem('ssid').val;

    var wifi_passwd = document.getElementById("passwd");
    wifi_passwd.value = findItem('passwd').val;

    var timestamp = document.getElementById("theTime");
    timestamp.value = findItem('timestamp').val;

    var run_webapp = document.getElementById("web_app_run");
    run_webapp.checked = findItem('web_app_run').val;

    var web_app_timeout = document.getElementById("web_app_timeout");
    web_app_timeout.value = findItem('web_app_timeout').val;

    var activity_mode = findItem('activity_mode').val;
    if (activity_mode == 1) {
        var activity_dates = document.getElementById("activity_dates");
        activity_dates.checked = true;
    }
    else if (activity_mode == 2) {
        var activity_lastmonth = document.getElementById("activity_lastmonth");
        activity_lastmonth.checked = true;

    }
    if (activity_mode == 3) {
        var activity_lastdwn = document.getElementById("activity_lastdwn");
        activity_lastdwn.checked = true;
    }
    if (activity_mode == 4) {
        var activity_all = document.getElementById("activity_all");
        activity_all.checked = true;
    }
}
function setpath() {
    var default_path = document.getElementById("newfile").files[0].name;
    document.getElementById("filepath").value = default_path;
}
function upload() {
    var filePath = document.getElementById("filepath").value;
    var upload_path = "/upload/" + filePath;
    var fileInput = document.getElementById("newfile").files;

    /* Max size of an individual file. Make sure this
    * value is same as that set in file_server.c */
    var MAX_FILE_SIZE = 500 * 1024;
    var MAX_FILE_SIZE_STR = "500KB";

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else if (filePath.length == 0) {
        alert("File path on server is not set!");
    } else if (filePath.indexOf(' ') >= 0) {
        alert("File path on server cannot have spaces!");
    } else if (filePath[filePath.length - 1] == '/') {
        alert("File name not specified after path!");
    } else if (fileInput[0].size > 500 * 1024) {
        alert("File size must be less than 200KB!");
    } else {
        document.getElementById("newfile").disabled = true;
        document.getElementById("filepath").disabled = true;
        document.getElementById("upload").disabled = true;

        var file = fileInput[0];
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
            if (xhttp.readyState == 4) {
                if (xhttp.status == 200) {
                    document.open();
                    document.write(xhttp.responseText);
                    document.close();
                } else if (xhttp.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload()
                } else {
                    alert(xhttp.status + " Error!\n" + xhttp.responseText);
                    location.reload()
                }
            }
        };
        xhttp.open("POST", upload_path, true);
        xhttp.send(file);
    }
}

function openTab(evt, name) {
    // Declare all variables
    var i, tabcontent, tablinks;

    // Get all elements with class="tabcontent" and hide them
    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }

    // Get all elements with class="tablinks" and remove the class "active"
    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }

    // Show the current tab, and add an "active" class to the button that opened the tab
    document.getElementById(name).style.display = "block";
    evt.currentTarget.className += " active";
}
function openFirstTab(name) {
    // Declare all variables
    var i, tabcontent, tablinks;

    // Get all elements with class="tabcontent" and hide them
    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }

    // Get all elements with class="tablinks" and remove the class "active"
    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }

    // Show the current tab, and add an "active" class to the button that opened the tab
    document.getElementById(name).style.display = "block";
    document.getElementById("tablink_ddd").className += " active";
}

removeElementsByLang();
populateSettings();
openFirstTab('Analysis');
populateStorageStatus();
