import * as ksu from "./ksu.js";
//环境
const debug = true;
if (!debug && window.ksu == null) {
    document.body.innerHTML = '<h1 style="text-align:center;">Only can working in KernelSU</h1>';
    throw new ReferenceError("Only can working in KernelSU")
    // return
}
/**
 * @type {Set<string>}
 */
let targetsSet;
//正在添加所有包
let addingAllPackages = false;
//笨办法
window.addEventListener("load", (event) => {
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
        await addTargetPackage(input.value);
        //追加元素
        document.getElementById("targetList").appendChild(createAppListItem(input.value));
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
})
/**
 * @description 添加目标包
 * @param {string} pkg 
 */
async function addTargetPackage(pkg) {
    //阻止操作
    maskVisibility(true);
    //防止重复
    targetsSet.add(pkg);
    let tempFile = new String();
    for (const name of targetsSet) {
        tempFile += `${name}\n`
    }
    await writeTargetFile(tempFile);
    maskVisibility(false);
}
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
    tablePackageName.innerText = pkgName;
    //证书生成启用检测
    if (pkgName.endsWith("!")) {
        tablePackageName.style.color = "green";
        //不让感叹号显示
        tablePackageName.innerText = pkgName.replace("!", "");
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
    item.append(tablePackageName, removeButton);
    return item
}
/**
 * @description 写入target.txt
 * @param {string} data 
 */
async function writeTargetFile(data) {
    await ksu.exec(`echo "${data}" > /storage/emulated/0/target.txt`);
}
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
    //修改列表元素
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