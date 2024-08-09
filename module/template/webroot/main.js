import * as ksu from "./ksu.js";
import util from "./util.js";
//环境检测
if (window.ksu == null) {
    document.body.innerHTML = '<h1 style="text-align:center;">Only can working in KernelSU</h1>';
    throw new ReferenceError("Only can working in KernelSU")
}
/**
 * @type {Set<string>}
 */
let targetsSet;
//正在添加所有包
let addingAllPackages = false;
/**
 * @description 应用列表和对应名称映射
 * @type {Map<string,string>}
 */
let appListMap;
//选择应用列表生成标记
let appSelectListCreated=false;
window.addEventListener("load", async (event) => {
    //文件存在检测
    //keybox.xml
    ksu.exec("test -e /data/adb/tricky_store/keybox.xml").then((result) => {
        const keystoreElement = document.getElementById("keystoreState");
        if (result.errno === 0) {
            keystoreElement.innerText = "✓已加载keybox.xml";
            keystoreElement.style.color = "green";
        } else {
            keystoreElement.innerText = "✕未找到keybox.xml";
            keystoreElement.style.color = "red";
        }
    });
    //spoof_build_vars
    ksu.exec("test -e /data/adb/tricky_store/spoof_build_vars").then((result) => {
        const spoofingElement = document.getElementById("varsSpoofingState");
        if (result.errno === 0) {
            spoofingElement.innerText = "✓已加载spoof_build_vars";
            spoofingElement.style.color = "green";
        } else {
            spoofingElement.innerText = "!未找到spoof_build_vars";
            spoofingElement.style.color = "coral";
        }
    });
    //缓存文件检测
    const hasAppListCache= await ksu.exec("test -e /data/adb/modules/tricky_store/webroot/cache/appList");
    //遮罩
    if (hasAppListCache.errno !== 0) {
        // 设置aapt权限
        await ksu.exec(`chmod 555 ${util.webDir}/lib/aapt`);
        await createAppListCache();
    }
    mask.innerText="正在读取应用列表"
    appListMap=await util.readCache();
    if (appListMap===null) {
        //读取失败
        mask.innerText="读取列表失败 将自动清除缓存并刷新"
        clearCache();
        return
    }
    //隐藏并清空
    mask.hidden=true;
    mask.innerText=""
    // target.txt检测及创建
    ksu.exec("test -e /data/adb/tricky_store/target.txt").then(async (result) => {
        if (result.errno !== 0) {
            //创建
            await ksu.exec("touch /data/adb/tricky_store/target.txt");
        }
        //读取
        const targets = await ksu.exec("cat /data/adb/tricky_store/target.txt");
        //空列表
        if (targets.stdout.length === 0) return
        //分割
        /**
         * @type {string[]}
         */
        //转到set
        targetsSet = new Set(targets.stdout.split("\n"));
        const fragment = document.createDocumentFragment();
        //构建列表
        for (const pkgName of targetsSet) {
            if (pkgName === "") continue;
            fragment.appendChild(createAppListItem(pkgName));
        };
        document.getElementById("targetList").appendChild(fragment);
    });
    //加应用
    document.getElementById("addPackageDialogConfirmButton").addEventListener("click", async () => {
        const input = document.getElementById("packageNameInput");
        //空输入
        if (input.value === "") {
            ksu.toast("无效包名");
            return
        }
        //启用生成证书时可以收到带'!'的新内容 避免下次加载才显示绿色包名
        const newPkgName=await addTargetPackage(input.value);
        //追加元素
        document.getElementById("targetList").appendChild(createAppListItem(newPkgName));
        //清空输入 关闭对话框
        document.getElementById('packageNameInput').value = '';
        document.getElementById('addPackageDialog').close();
    });
    //添加全部滑杆 归位
    document.getElementById('addAllAppsConfirmSlider').addEventListener('touchend', (event) => {
        event.target.value = 0;
    });
    //添加全部滑杆 滑动
    document.getElementById('addAllAppsConfirmSlider').addEventListener('touchmove', (event) => {
        //到头了
        if (event.target.value == 100) {
            document.getElementById("addAllAppsDialog").close();
            addAllApps();
        }
    });
    //加应用按钮
    document.getElementById("addPackageButton").addEventListener("click",async ()=>{
        document.getElementById("addPackageAppSelectDialog").show();
        if(!appSelectListCreated) createAppSelectList();
    });
    document.getElementById("clearCacheButton").addEventListener("click", async () => {
        document.getElementById("clearCacheDialog").close();
        maskVisibility(true);
        clearCache();
    });
})
/**
 * @description 添加目标包
 * @param {string} pkg 
 * @param {boolean|null} enableGenerateCertificate
 */
async function addTargetPackage(pkg,enableGenerateCertificate) {
    // 检查重复 有些加了"!"的一起查
    if(targetsSet.has(pkg) || targetsSet.has(pkg+"!")) {
        ksu.toast("已添加过该应用");
        return
    }
    //阻止操作
    maskVisibility(true);
    //防止重复
    if (enableGenerateCertificate??document.getElementById("generateCertificateCheckbox").checked) {
        pkg+="!";
        //不复位了 基本上下一次也要
    }
    targetsSet.add(pkg);
    let tempFile = new String();
    for (const name of targetsSet) {
        tempFile += `${name}\n`
    }
    await writeTargetFile(tempFile);
    maskVisibility(false);
    return pkg
}
/**
 * @description 移除目标包
 * @param {string} pkg 目标包名 
 */
async function removeTargetPackage(pkg) {
    maskVisibility(true);
    targetsSet.delete(pkg);
    let tempFile = new String();
    for (const name of targetsSet) {
        tempFile += `${name}\n`
    }
    await writeTargetFile(tempFile);
    maskVisibility(false);
}
/**
 * @description 遮罩层
 * @param {boolean} show 
 */
function maskVisibility(show) {
    document.getElementById("mask").hidden = !show;
}
/**
 * @description 创建应用列表行元素
 * @param {string} pkgName 
 */
function createAppListItem(pkgName) {
    //行本身
    const item = document.createElement("div");
    item.setAttribute("package", pkgName)
    item.style.marginTop = "10px"
    //包名
    const tablePackageName = document.createElement("span");
    //显示
    tablePackageName.innerText = appListMap.get(pkgName.replace("!", "")) ?? pkgName;
    //证书生成启用检测
    if (pkgName.endsWith("!")) {
        tablePackageName.style.color = "green";
        //不让感叹号显示
        tablePackageName.innerText = tablePackageName.innerText.replace("!", "");
    }
    //按钮
    const removeButton = document.createElement("button");
    removeButton.setAttribute("package", pkgName)
    removeButton.addEventListener("click", async (event) => {
        removeTargetPackage(event.target.getAttribute("package"), targetsSet);
        //移除元素
        event.target.parentElement.remove()
    })
    removeButton.innerText = "移除";
    removeButton.style.float = "right";
    removeButton.classList.add("baseButton")
    item.append(tablePackageName, removeButton);
    return item
}
/**
 * @description 写入target.txt
 * @param {string} data 
 */
async function writeTargetFile(data) {
    await ksu.exec(`echo "${data}" > /data/adb/tricky_store/target.txt`);
}
/**
 * @description 添加所有用户应用
 */
async function addAllApps() {
    if (addingAllPackages) return
    addingAllPackages = true;
    maskVisibility(true);
    /**
     * @type {{stdout:string}}
     */
    const allUserAppsList = await ksu.exec(`pm list packages -3`);
    const listArray = allUserAppsList.stdout.replaceAll("package:", "").split("\n");
    targetsSet = new Set(listArray);
    //重写文件
    let tmp = new String();
    for (const pkg of targetsSet) {
        tmp += `${pkg}\n`
    }
    await writeTargetFile(tmp);
    //清空列表元素
    document.getElementById("targetList").innerHTML = "";
    const fragment = document.createDocumentFragment();
    //构建列表
    for (const pkgName of targetsSet) {
        fragment.appendChild(createAppListItem(pkgName));
    };
    document.getElementById("targetList").appendChild(fragment);
    maskVisibility(false);
    ksu.toast("成功添加全部用户应用");
    addingAllPackages=false;
}
/**
 * @description 创建应用选择列表
 */
async function createAppSelectList(){
    // 标记 修复内容重复
    appSelectListCreated=true;
    const fragment=document.createDocumentFragment();
    /* 系统应用一般没必要加
    而且如果堆一起在列表里会很长 那么分类显示和搜索至少加一个不然基本没法用
    (说白了就是懒)
    */
    const allUserAppsList = await ksu.exec(`pm list packages -3`);
    /**
     * @type {string[]}
     */
    const listArray = allUserAppsList.stdout.replaceAll("package:", "").split("\n");
    listArray.forEach((pkgName)=>{
        const listElement=document.createElement("div");
        // 不然根本点不准
        listElement.style.marginTop="5px";
        listElement.style.marginBottom="5px";
        listElement.innerText=appListMap.get(pkgName) || pkgName;
        listElement.addEventListener("click",async ()=>{
            if(targetsSet.has(pkgName) || targetsSet.has(pkgName + "!")) {
                ksu.toast("已添加过该应用");
                return
            };
            const certCheckboxChecked = document.getElementById("generateCertificateCheckboxInSelectList").checked;
            document.getElementById("targetList").appendChild(createAppListItem(pkgName+(certCheckboxChecked?"!":"")));
            await addTargetPackage(pkgName,certCheckboxChecked);
            document.getElementById("addPackageAppSelectDialog").close();
        });
        fragment.appendChild(listElement);
    })
    document.getElementById("addPackageSelect").appendChild(fragment);
}
/**
 * @description 创建列表缓存
 */
async function createAppListCache(){
    await ksu.exec(`sh ${util.webDir}/shell/init.sh`);
}
/**
 * @description 清理缓存并在3.5秒后刷新
 */
async function clearCache() {
    await ksu.exec("rm -f /data/adb/modules/tricky_store/webroot/cache/appList");
    ksu.toast("缓存清理成功");
    setTimeout(() => {
        location.reload();
    }, 3500);
}