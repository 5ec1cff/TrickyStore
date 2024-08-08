import * as ksu from "./ksu.js";
class aaptUtil {
    //模块目录
    static webDir = "/data/adb/modules/tricky_store/webroot";
    /**
     * @description 读取缓存
     * @returns {Map<string,string>}
     */
    static async readCache(){
        /**
         * @type {{stdout:string,stderr:number}}
         */
        const result=await ksu.exec(`cat ${this.webDir}/cache/appList`);
        if (result.errno!==0) {
            //异常
            return null
        }
        /**
         * @description <PackageName, AppName>
         * @type {Map<string,string>}
         */
        const appMap=new Map();
        //读取到的缓存列表文本
        const listArray=result.stdout.split("\n");
        for (const appData of listArray) {
            const pkgName=appData.slice(0,appData.indexOf(">"));
            let appName=appData.slice(appData.indexOf(">")+1);
            //防止纯空格 好像有这种情况
            //好吧是误判 不过留着吧
            // if(appName.replaceAll(" ","")==="") appName=pkgName
            appMap.set(pkgName,appName)
        }
        return appMap;
    }
}
export default aaptUtil;