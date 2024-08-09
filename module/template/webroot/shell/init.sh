workDir="/data/adb/modules/tricky_store";
aaptPath=$workDir/webroot/lib/aapt;
packageList=$(pm list packages);
mkdir -p $workDir/webroot/cache/;
# 删除旧文件
rm -f $workDir/webroot/cache/appList;
for package in $packageList; do
    # 包名
    packageName=${package#package:};
    packageName=${packageName%%/*};
    # apk路径
    packagePath=$(pm path $packageName | cut -d ":" -f 2);
    # 先尝试获取中文
    appName=$($aaptPath dump badging $packagePath | grep -i "application-label-zh-CN" | cut -d ":" -f 2 | cut -d "'" -f 2);
    # 为空取label
    if [ -z "$appName" ]; then
    # 应用名
        appName=$($aaptPath dump badging $packagePath | grep -i "application-label:" | cut -d ":" -f 2 | cut -d "'" -f 2);
    fi
    # # 再空则取包名
    if [ -z "$appName" ]; then
        appName=$packageName;
    fi
    echo -e "${packageName}>${appName}" >> $workDir/webroot/cache/appList;
done