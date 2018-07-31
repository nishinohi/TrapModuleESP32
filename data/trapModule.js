const KEY_WORK_TIME = 'WorkTime';
const KEY_TRAP_MODE = 'TrapMode';
const KEY_NODE_ID = 'NodeId';
const KEY_NODE_LIST = 'NodeList';
const KEY_TRAP_FIRE_MESSAGE = 'TrapFireMessage';
const KEY_BATTERY_DEAD = 'BatteryDead';
const KEY_PARENT_NODE_ID = 'ParentNodeId';
const KEY_PICTURE = 'CameraImage';
const KEY_CAMERA_ENABLE = 'CameraEnable';
const KEY_TRAP_FIRE = 'TrapFire';
const KEY_GPS_LAT = 'GpsLat';
const KEY_GPS_LON = 'GpsLon';
const KEY_MESH_GRAPH = 'MeshGraph';
const KEY_ACTIVE_START = 'ActiveStart';
const KEY_ACTIVE_END = 'ActiveEnd';
const KEY_CURRENT_TIME = 'CurrentTime';
const KEY_PICTURE_FORMAT = 'PictureFormat'
const KEY_IS_PARENT = 'IsParent';

var sigmaUuid = 0;
var sigmaObj;
var sliderObj;

// 現在時刻
var timerId = -1;
var localEpoch = 0;

window.onload = function () {
    let succesPop = document.querySelector('.successBox');
    succesPop.addEventListener('transitionend', function (evt) {
        let style = getComputedStyle(succesPop);
        if (style.opacity == 1) {
            succesPop.classList.toggle('show');
        }
    });
    let failedPop = document.querySelector('.failedBox');
    failedPop.addEventListener('transitionend', function (evt) {
        let style = getComputedStyle(failedPop);
        if (style.opacity == 1) {
            failedPop.classList.toggle('show');
        }
    });
    sigmaObj = new sigma({
        settings: {
            verbose: false
        },
        renderer: {
            container: 'graph-container',
            type: 'canvas'
        }
    });
    getModuleInfo();
    sliderObj = new rSlider({
        target: '#slider',
        values: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24],
        range: true, // range slider
        tooltip: false,
        onChange: checkConfig
    });
}

/**
 * メッシュネットワークのjsonからsigmaJsonに変換
 * @param {*} graph 
 * @param {*} ownNodeId 
 * @param {*} meshes 
 */
function parsePainlessJson(graph, ownNodeId, meshes) {
    meshes.forEach(function (mesh) {
        if (graph.nodes(mesh.nodeId) === undefined) {
            graph.addNode({
                id: mesh.nodeId,
                label: 'Node ' + mesh.nodeId,
                x: Math.random(),
                y: Math.random(),
                size: 1,
                color: '#617db4'
            });
        }
        graph.addEdge({
            id: ++sigmaUuid,
            source: ownNodeId,
            target: mesh.nodeId,
            size: Math.random(),
            type: 't'
        });
        if (mesh.subs != null) {
            parsePainlessJson(graph, mesh.nodeId, mesh.subs);
        }
    });
    return null;
}

/**
 * メッシュネットワークグラフを作成
 * @param {*} response 
 */
function createMeshGraph(response) {
    let srcJson = JSON.parse(response);
    sigmaObj.graph.clear();
    // 最初のメッシュを作成
    let nodeId = document.getElementById(KEY_NODE_ID).textContent;
    if (nodeId == null) {
        return;
    }
    sigmaObj.graph.addNode({
        id: 0,
        label: 'Node ' + nodeId,
        x: Math.random(),
        y: Math.random(),
        size: 1,
        color: '#617db4'
    });
    parsePainlessJson(sigmaObj.graph, 0, srcJson)
    sigmaObj.refresh();
    // Start the ForceAtlas2 algorithm:
    sigmaObj.startForceAtlas2();
    let autoPosition = function () {
        sigmaObj.stopForceAtlas2();
    };
    setTimeout(autoPosition, 2500);
    let dragListener = sigma.plugins.dragNodes(sigmaObj, sigmaObj.renderers[0]);
}

/**
 * Validation Chaeck
 * @param {*} elem 
 */
function checkConfig(values) {
    let sliderMin = !values ? sliderObj.getValue().split(',')[0] : values.split(',')[0];
    let sliderMax = !values ? sliderObj.getValue().split(',')[1] : values.split(',')[1];
    let form = document.getElementById('moduleSetting');
    document.getElementById('sendButton').disabled =
        !form.checkValidity() || (sliderMin == sliderMax);
}

/**
 * GET メソッドの返り値に対して何かしらの処理を実行するテンプレート関数
 * callBack 関数名は GET リクエストの url と一致させること
 */
function getAndDoAfter(callBack, url) {
    if (callBack === null) {
        console.error('method does not defined')
        return;
    }
    let funcName = callBack.name;
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            callBack(this.responseText);
            togglePopup('.successBox');
        } else if (this.readyState == 4 && this.status != 200) {
            console.error('status:' + String(this.status));
            togglePopup('.failedBox');
        }
    };
    xhr.open('GET', '/' + url, true);
    xhr.send();
}

/**
 * POST メソッドの返り値に対して何かしらの処理を実行するテンプレート関数
 * callBack 関数名は POST リクエストの url と一致させること
 */
function postAndDoAfter(postElements, callBack, url) {
    if (callBack === null) {
        console.error('method does not defined')
        return;
    }
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            callBack(this.responseText);
            togglePopup('.successBox');
        } else if (this.readyState == 4 && this.status != 200) {
            console.error('status:' + String(this.status));
            togglePopup('.failedBox');
        }
    };
    xhr.open('POST', '/' + url, true);
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
    xhr.send(EncodeHTMLForm(postElements));
}

function togglePopup(selector) {
    let popupElement = document.querySelector(selector);
    if (popupElement != null && document.querySelector(selector + '.show') == null) {
        popupElement.classList.toggle('show');
    }
}

/**
 * モジュール情報取得
 */
function getModuleInfo() {
    getAndDoAfter(updateModuleInfo, getModuleInfo.name);
    getMeshGraph();
}

/**
 * メッシュグラフ情報取得
 */
function getMeshGraph() {
    getAndDoAfter(createMeshGraph, getMeshGraph.name);
}

/**
 * 現在時刻設定
 */
function setCurrentTime() {
    let current = new Date();
    // モジュールは現在時刻を UTC(sec) で管理しているので UTC での表示時刻が local での表示時刻と一致するようにずらす
    let tempLocalEpoch = parseInt((current.getTime() - (current.getTimezoneOffset() * 60 * 1000)) / 1000);
    let currentTime = { CurrentTime: tempLocalEpoch };
    postAndDoAfter(currentTime, updateTime, setCurrentTime.name);
}

/**
 * 設定値送信
 */
function setConfig() {
    let config = {
        WorkTime: document.getElementById('workTimeConfig').value,
        TrapMode: document.getElementById('modeConfig').value,
        ActiveStart: sliderObj.getValue().split(',')[0],
        ActiveEnd: sliderObj.getValue().split(',')[1]
    };
    postAndDoAfter(config, updateModuleInfo, setConfig.name);
}

/**
 * テストメッセージ送信
 */
function sendMessage() {
    let dbgMsg = {
        messageContent: document.getElementById('messageContent').value,
        messageSendNodeId: document.getElementById('messageSendNodeId').value
    };
    postAndDoAfter(dbgMsg, function (param) { }, sendMessage.name);
}

/**
 * 画像取得
 */
function snapShot() {
    let picFmt = { PictureFormat: document.getElementById('PictureFormat').value };
    postAndDoAfter(picFmt, updateImage, snapShot.name);
}

/**
 * メッセージ送信タイプ変更(broadcast or single)
 */
function changeMessageSendType() {
    if (document.getElementById('messageSendType').value == 0) {
        document.getElementById('messageSendNodeIdList').style.display = 'block';
    } else {
        document.getElementById('messageSendNodeIdList').style.display = 'none';
    }
}

/**
 * GPS 初期化
 */
function initGps() {
    getAndDoAfter(function (param) {
        document.getElementById(KEY_GPS_LAT).textContent = "";
        document.getElementById(KEY_GPS_LON).textContent = "";
        document.getElementById('TrapLocation').removeAttribute('href');
    }, initGps.name);
}
/**
 * GPS 取得
 */
function getGps() {
    getAndDoAfter(function (param) { }, getGps.name);
}

/**
 * モジュール情報更新
 */
function updateModuleInfo(response) {
    let config = JSON.parse(response);
    for (let key in config) {
        if (config[key] == null || document.getElementById(key) == null) {
            continue;
        }
        switch (key) {
            case KEY_TRAP_MODE:
                document.getElementById(key).textContent = config[key] ? 'Trap Mode' : 'Set Mode';
                break;
            case KEY_TRAP_FIRE:
                document.getElementById(key).textContent = config[key] ? 'Fired' : 'Not Fired';
                break;
            case KEY_CAMERA_ENABLE:
                document.getElementById(key).style.display = config[key] ? 'initial' : 'none';
                break;
            case KEY_NODE_LIST:
                updateTargetList(key, config[key]);
                break;
            case KEY_MESH_GRAPH:
                createMeshGraph(config[key]);
                break;
            case KEY_CURRENT_TIME:
                updateTime(config[key]);
                break;
            default:
                document.getElementById(key).textContent = config[key];
        }
    }
    if (config[KEY_GPS_LAT] != null && config[KEY_GPS_LON] != null) {
        let trapLocation = document.getElementById('TrapLocation');
        if (config[KEY_GPS_LAT].length == 0 || config[KEY_GPS_LON].length == 0) {
            trapLocation.removeAttribute('href');
        } else {
            trapLocation.setAttribute('href', 'http://maps.google.com/maps?q=' + config[KEY_GPS_LAT] + ',' + config[KEY_GPS_LON]);
        }
    }
}

/**
 * 任意のリスト情報を更新
 * @param {*} targetId リストのID
 * @param {*} listDatas リストデータ
 */
function updateTargetList(targetId, listDatas) {
    let targetList = document.getElementById(targetId);
    if (!targetList) {
        return;
    }
    while (targetList.firstChild) {
        targetList.removeChild(targetList.firstChild);
    }
    for (let listData of listDatas) {
        let li = document.createElement('li');
        li.textContent = listData;
        targetList.appendChild(li);
    }
}

/**
 * 画像情報更新
 * @param {*} src 
 */
function updateImage(src) {
    let pictureList = document.getElementById('picture');
    if (pictureList == null || src == null || src == '') {
        return;
    }
    // 現在の画像ファイルを初期化
    while (pictureList.firstChild) {
        pictureList.removeChild(pictureList.firstChild);
    }
    let li = document.createElement('li');
    li.style = 'list-style-type: none';
    let img = document.createElement('img');
    img.border = 0;
    img.src = src;
    img.alt = 'not found';
    li.appendChild(img);
    pictureList.appendChild(li);
}

/**
 * 現在時刻設定
 * コールバック関数として呼び出されるので引数を設定しているが使用しない
 */
function updateTime(param) {
    let moduleTime = document.getElementById('CurrentTime').firstElementChild;
    if (moduleTime != null) {
        if (timerId !== -1) {
            clearInterval(timerId);
        }
        // モジュールは UTC Epoch(sec)で値を保持しているので local time に変換
        localEpoch = param === "" ? 0 : param * 1000 + (new Date().getTimezoneOffset() * 60 * 1000);
        timerId = setInterval(function () {
            let now = param === "" ? new Date() : new Date(localEpoch);
            moduleTime.textContent = now.getFullYear().toString() + "/" + (now.getMonth() + 1).toString() + "/" + now.getDate().toString() + " " + now.toLocaleTimeString();
            localEpoch = param === "" ? localEpoch : localEpoch + 1000;
        }, 1000);
    }
}

function EncodeHTMLForm(data) {
    let params = [];
    for (let name in data) {
        if (data[name] == '' || data[name] == null) {
            continue;
        }
        let value = data[name];
        let param = encodeURIComponent(name) + '=' + encodeURIComponent(value);
        params.push(param);
    }
    return params.join('&').replace(/%20/g, '+');
}