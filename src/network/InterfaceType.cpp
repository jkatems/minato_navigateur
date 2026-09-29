#include "InterfaceType.h"
#include <QFileInfo>
QString interfaceType(const QNetworkInterface &iface) {
    if (iface.type() == QNetworkInterface::Wifi)
        return "Wi-Fi";
#ifdef Q_OS_LINUX
    // Linux may report ARPHRD_ETHER for Wi-Fi; sysfs supplies the missing distinction.
    if (QFileInfo("/sys/class/net/" + iface.name() + "/wireless").isDir())
        return "Wi-Fi";
#endif
    if (iface.type() == QNetworkInterface::Ethernet)
        return "Ethernet / liaison virtuelle";
    if (iface.type() == QNetworkInterface::Loopback)
        return "Boucle locale";
    return "Autre / indéterminé";
}
