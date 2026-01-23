#ifndef LAYERMAPPER_H
#define LAYERMAPPER_H

#include <QString>
#include <QMap>
#include <QStringList>

namespace EasyKiConverter
{

    /**
     * @brief 图层映射器类
     *
     * 用于将嘉立创 EDA 图层映射�?KiCad 图层，并处理单位转换
     */
    class LayerMapper
    {
    public:
        /**
         * @brief KiCad 图层枚举
         */
        enum KiCadLayer
        {
            // 信号�?
            F_Cu = 0,     // 顶层�?
            In1_Cu = 1,   // 内层1
            In2_Cu = 2,   // 内层2
            In3_Cu = 3,   // 内层3
            In4_Cu = 4,   // 内层4
            In5_Cu = 5,   // 内层5
            In6_Cu = 6,   // 内层6
            In7_Cu = 7,   // 内层7
            In8_Cu = 8,   // 内层8
            In9_Cu = 9,   // 内层9
            In10_Cu = 10, // 内层10
            In11_Cu = 11, // 内层11
            In12_Cu = 12, // 内层12
            In13_Cu = 13, // 内层13
            In14_Cu = 14, // 内层14
            In15_Cu = 15, // 内层15
            In16_Cu = 16, // 内层16
            In17_Cu = 17, // 内层17
            In18_Cu = 18, // 内层18
            In19_Cu = 19, // 内层19
            In20_Cu = 20, // 内层20
            In21_Cu = 21, // 内层21
            In22_Cu = 22, // 内层22
            In23_Cu = 23, // 内层23
            In24_Cu = 24, // 内层24
            In25_Cu = 25, // 内层25
            In26_Cu = 26, // 内层26
            In27_Cu = 27, // 内层27
            In28_Cu = 28, // 内层28
            In29_Cu = 29, // 内层29
            In30_Cu = 30, // 内层30
            B_Cu = 31,    // 底层�?

            // 丝印�?
            F_SilkS = 32, // 顶层丝印
            B_SilkS = 33, // 底层丝印

            // 阻焊�?
            F_Mask = 34, // 顶层阻焊
            B_Mask = 35, // 底层阻焊

            // 助焊�?
            F_Paste = 36, // 顶层锡膏
            B_Paste = 37, // 底层锡膏

            // 粘合�?
            F_Adhes = 38, // 顶层粘合
            B_Adhes = 39, // 底层粘合

            // 边缘�?
            Edge_Cuts = 44, // 板框轮廓

            // 边界�?
            F_CrtYd = 45, // 顶层边界
            B_CrtYd = 46, // 底层边界

            // 装配�?
            F_Fab = 47, // 顶层装配
            B_Fab = 48, // 底层装配

            // 文档�?
            Dwgs_User = 49, // 用户绘图
            Cmts_User = 50, // 用户注释
            Eco1_User = 51, // 用户1
            Eco2_User = 52, // 用户2

            // 边缘�?
            Margin = 53, // 边缘

            // 用户自定义层
            User_1 = 54,
            User_2 = 55,
            User_3 = 56,
            User_4 = 57,
            User_5 = 58,
            User_6 = 59,
            User_7 = 60,
            User_8 = 61,
            User_9 = 62
        };

        /**
         * @brief 构造函�?
         */
        LayerMapper();

        /**
         * @brief 析构函数
         */
        ~LayerMapper();

        /**
         * @brief 将嘉立创 EDA 图层 ID 映射�?KiCad 图层
         *
         * @param easyedaLayerId 嘉立�?EDA 图层 ID
         * @return int KiCad 图层 ID，如果无法映射则返回 -1
         */
        int mapToKiCadLayer(int easyedaLayerId) const;

        /**
         * @brief 将嘉立创 EDA 图层名称映射�?KiCad 图层
         *
         * @param easyedaLayerName 嘉立�?EDA 图层名称
         * @return int KiCad 图层 ID，如果无法映射则返回 -1
         */
        int mapToKiCadLayer(const QString &easyedaLayerName) const;

        /**
         * @brief 获取 KiCad 图层名称
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return QString KiCad 图层名称
         */
        QString getKiCadLayerName(int kicadLayerId) const;

        /**
         * @brief �?mil 转换�?mm
         *
         * @param milValue mil �?
         * @return double mm �?
         */
        static double milToMm(double milValue);

        /**
         * @brief �?mm 转换�?mil
         *
         * @param mmValue mm �?
         * @return double mil �?
         */
        static double mmToMil(double mmValue);

        /**
         * @brief 判断图层是否为信号层
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return bool 是否为信号层
         */
        static bool isSignalLayer(int kicadLayerId);

        /**
         * @brief 判断图层是否为丝印层
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return bool 是否为丝印层
         */
        static bool isSilkLayer(int kicadLayerId);

        /**
         * @brief 判断图层是否为阻焊层
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return bool 是否为阻焊层
         */
        static bool isMaskLayer(int kicadLayerId);

        /**
         * @brief 判断图层是否为助焊层
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return bool 是否为助焊层
         */
        static bool isPasteLayer(int kicadLayerId);

        /**
         * @brief 判断图层是否为机械层
         *
         * @param kicadLayerId KiCad 图层 ID
         * @return bool 是否为机械层
         */
        static bool isMechanicalLayer(int kicadLayerId);

        /**
         * @brief 判断是否为元件外形层（边界层�?
         *
         * 元件外形层用于布局避让，应映射�?F.CrtYd �?B.CrtYd
         * 参�?LCKiConverter: src/jlc/pro_footprint.ts isCourtYard()
         *
         * @param easyedaLayerId 嘉立�?EDA 图层 ID
         * @return bool 是否为元件外形层
         */
        static bool isCourtYardLayer(int easyedaLayerId);

        /**
         * @brief 判断是否为内�?
         *
         * @param easyedaLayerId 嘉立�?EDA 图层 ID
         * @return bool 是否为内�?
         */
        static bool isInnerLayer(int easyedaLayerId);

        /**
         * @brief 判断是否为焊盘层
         *
         * @param easyedaLayerId 嘉立�?EDA 图层 ID
         * @return bool 是否为焊盘层
         */
        static bool isPadLayer(int easyedaLayerId);

        /**
         * @brief 获取元件外形层对应的 KiCad 图层
         *
         * 元件外形层应映射�?F.CrtYd（顶层）�?B.CrtYd（底层）
         * 参�?LCKiConverter: src/jlc/pro_footprint.ts isCourtYard()
         *
         * @param easyedaLayerId 嘉立�?EDA 图层 ID
         * @return QString KiCad 图层名称，如果不是元件外形层则返回空字符�?
         */
        static QString getCourtYardLayerName(int easyedaLayerId);

        /**
         * @brief 获取图层映射说明
         *
         * @return QString 映射说明
         */
        QString getMappingDescription() const;

    private:
        /**
         * @brief 初始化图层映射表
         */
        void initializeLayerMapping();

    private:
        QMap<int, int> m_layerIdMapping;       // 嘉立创图�?ID -> KiCad 图层 ID
        QMap<QString, int> m_layerNameMapping; // 嘉立创图层名�?-> KiCad 图层 ID
        QMap<int, QString> m_kicadLayerNames;  // KiCad 图层 ID -> 图层名称
    };

} // namespace EasyKiConverter

#endif // LAYERMAPPER_H
