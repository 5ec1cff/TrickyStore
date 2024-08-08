workDir="/data/adb/modules/tricky_store";
aaptPath=$workDir/webroot/lib/aapt;
packageList=$(pm list packages);
mkdir -p $workDir/webroot/cache/;
# 删除旧文件
rm -f $workDir/webroot/cache/appList;
for package in $packageList; do
    packageName=${package#package:};
    packageName=${packageName%%/*};
    packagePath=$(pm path $packageName | cut -d ":" -f 2);
    # 先尝试获取中文
    # 没用
    # appName=$($aaptPath dump badging $packagePath | grep -i "application-label-zh:" | cut -d ":" -f 2 | cut -d "'" -f 2);
    # 为空取label
    # if [ -z "$appName" ]; then
    appName=$($aaptPath dump badging $packagePath | grep -i "application-label:" | cut -d ":" -f 2 | cut -d "'" -f 2);
    # fi
    # # 再空则取包名
    if [ -z "$appName" ]; then
        appName=$packageName;
    fi
    echo -e "${packageName}>${appName}" >> $workDir/webroot/cache/appList;
done