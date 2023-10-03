
//var tableData = [{name: 'Onion', quantity: 292, expiry: '2021-09-12'}, {name: 'Apple', quantity: 55, expiry: '2021-09-22'}, {name: 'Potato', quantity: 25, expiry: '2021-09-18'}, {name: 'Carrot', quantity: 8, expiry: '2021-09-25'}];

var table = document.getElementById('mytable');
var input = document.getElementById('myinput');
var caretUpClassName = '&uarr;';
var caretDownClassName = '&darr;';
var result = [];

const sort_by = (field, reverse, primer) => {

  const key = primer ?
    function (x) {
      return primer(x[field]);
    } :
    function (x) {
      return x[field];
    };

  reverse = !reverse ? 1 : -1;

  return function (a, b) {
    return a = key(a), b = key(b), reverse * ((a > b) - (b > a));
  };
};


function clearArrow() {
  let carets = document.getElementsByClassName('caret');
  for (let caret of carets) {
    caret.className = "caret";
  }
}


function toggleArrow(event) {
  let element = event.target;
  let caret, field, reverse;
  if (element.tagName === 'SPAN') {
    caret = element.getElementsByClassName('caret')[0];
    field = element.id
  }
  else {
    caret = element;
    field = element.parentElement.id
  }

  let iconClassName = caret.className;
  clearArrow();
  if (iconClassName.includes(caretUpClassName)) {
    caret.className = `caret ${caretDownClassName}`;
    reverse = false;
  } else {
    reverse = true;
    caret.className = `caret ${caretUpClassName}`;
  }

  tableData.sort(sort_by(field, reverse));
  populateTable();
}


function populateTable() {
  table.innerHTML = '';
  for (let data of tableData) {
    let row = table.insertRow(-1);

    let select = row.insertCell(0);
    select.innerHTML = `<input type="checkbox" id="check_file" name="${data.name}" value="${data.link}"> `;

    let name = row.insertCell(1);
    name.innerHTML = `<a href="${data.link}">${data.name}</a>`;

    let size = row.insertCell(2);
    size.innerHTML = data.size;

    let mdate = row.insertCell(3);
    mdate.innerHTML = data.mdate;

    let actions = row.insertCell(4);
    actions.innerHTML = "<form method=\"post\" action=\"/delete".concat(data.link, "\"><button type=\"submit\">Delete</button></form>");
  }

  filterTable();
}


function filterTable() {
  let filter = input.value.toUpperCase();
  rows = table.getElementsByTagName("TR");
  let flag = false;

  for (let row of rows) {
    let cells = row.getElementsByTagName("TD");
    for (let cell of cells) {
      if (cell.textContent.toUpperCase().indexOf(filter) > -1) {
        if (filter) {
          cell.style.backgroundColor = 'azure';
        } else {
          cell.style.backgroundColor = '';
        }

        flag = true;
      } else {
        cell.style.backgroundColor = '';
      }
    }

    if (flag) {
      row.style.display = "";
    } else {
      row.style.display = "none";
    }

    flag = false;
  }
}


populateTable();

let tableColumns = document.getElementsByClassName('table-column');

for (let column of tableColumns) {
  column.addEventListener('click', function (event) {
    toggleArrow(event);
  });
}

input.addEventListener('keyup', function (event) {
  filterTable();
});

/**
 * Download a list of files.
 * @author speedplane
 */
function download_files(table) {
  function download_next(i) {
    if (i >= table.length) {
      return;
    }
    var a = document.createElement('a');
    a.href = table[i].link;
    a.target = '_blank';
    // Use a.download if available, it prevents plugins from opening.
    if ('download' in a) {
      a.download = table[i].name;
    }
    // Add a to the doc for click to work.
    (document.body || document.documentElement).appendChild(a);
    if (a.click) {
      a.click(); // The click method is supported by most browsers.
    } else {
      $(a).click(); // Backup using jquery
    }
    // Delete the temporary link.
    a.parentNode.removeChild(a);
    // Download the next file with a small timeout. The timeout is necessary
    // for IE, which will otherwise only download the first file.
    setTimeout(function () {
      download_next(i + 1);
    }, 500);
  }
  // Initiate the first download.
  download_next(0);
}

function downloadChecked() {
  var checkboxes = document.querySelectorAll('input[type=checkbox]');
  for (let checkbox of checkboxes) {
    if (checkbox.checked) {
      var a = document.createElement('a');
      a.href =  checkbox.value;
      a.target = '_blank';
      // Use a.download if available, it prevents plugins from opening.
      if ('download' in a) {
        a.download = checkbox.name;
      }
      // Add a to the doc for click to work.
      (document.body || document.documentElement).appendChild(a);
      if (a.click) {
        a.click(); // The click method is supported by most browsers.
      } else {
        $(a).click(); // Backup using jquery
      }
      // Delete the temporary link.
      a.parentNode.removeChild(a);
      // Download the next file with a small timeout. The timeout is necessary
      // for IE, which will otherwise only download the first file.
      setTimeout(function () {;
      }, 500);
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
populateStorageStatus();