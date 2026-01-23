#include "LayerMapper.h"
#include <QDebug>

namespace EasyKiConverter
{

    // 单位转换常量
    const double MIL_TO_MM = 0.0254;  // 1 mil = 0.0254 mm
    const double MM_TO_MIL = 39.3701; // 1 mm = 39.3701 mil

    LayerMapper::LayerMapper()
    {
        initializeLayerMapping();
    }

    LayerMapper::~LayerMapper()
    {
    }

    void LayerMapper::initializeLayerMapping()
    {
        // 初始�?KiCad 图层名称
        m_kicadLayerNames[F_Cu] = "F.Cu";
        m_kicadLayerNames[B_Cu] = "B.Cu";
        for (int i = 1; i <= 30; i++)
        {
            m_kicadLayerNames[i] = QString("In%1.Cu").arg(i);
        }
        m_kicadLayerNames[F_SilkS] = "F.SilkS";
        m_kicadLayerNames[B_SilkS] = "B.SilkS";
        m_kicadLayerNames[F_Mask] = "F.Mask";
        m_kicadLayerNames[B_Mask] = "B.Mask";
        m_kicadLayerNames[F_Paste] = "F.Paste";
        m_kicadLayerNames[B_Paste] = "B.Paste";
        m_kicadLayerNames[F_Adhes] = "F.Adhes";
        m_kicadLayerNames[B_Adhes] = "B.Adhes";
        m_kicadLayerNames[Edge_Cuts] = "Edge.Cuts";
        m_kicadLayerNames[F_CrtYd] = "F.CrtYd";
        m_kicadLayerNames[B_CrtYd] = "B.CrtYd";
        m_kicadLayerNames[F_Fab] = "F.Fab";
        m_kicadLayerNames[B_Fab] = "B.Fab";
        m_kicadLayerNames[Dwgs_User] = "Dwgs.User";
        m_kicadLayerNames[Cmts_User] = "Cmts.User";
        m_kicadLayerNames[Eco1_User] = "Eco1.User";
        m_kicadLayerNames[Eco2_User] = "Eco2.User";
        m_kicadLayerNames[Margin] = "Margin";
        for (int i = 1; i <= 9; i++)
        {
            m_kicadLayerNames[User_1 + i - 1] = QString("User.%1").arg(i);
        }

        // 初始化嘉立创图层 ID -> KiCad 图层 ID 映射
        // 一、电气信号层
        m_layerIdMapping[1] = F_Cu; // TopLayer -> F.Cu
        m_layerIdMapping[2] = B_Cu; // BottomLayer -> B.Cu

        // 内电�?Inner1~Inner32 (ID 21-52) -> In1.Cu~In32.Cu
        for (int i = 0; i < 32; i++)
        {
            m_layerIdMapping[21 + i] = In1_Cu + i;
        }

        // 二、丝印层
        m_layerIdMapping[3] = F_SilkS; // TopSilkLayer -> F.SilkS
        m_layerIdMapping[4] = B_SilkS; // BottomSilkLayer -> B.SilkS

        // 三、阻焊层
        m_layerIdMapping[7] = F_Mask; // TopSolderMaskLayer -> F.Mask
        m_layerIdMapping[8] = B_Mask; // BottomSolderMaskLayer -> B.Mask

        // 四、助焊层
        m_layerIdMapping[5] = F_Paste; // TopPasteMaskLayer -> F.Paste
        m_layerIdMapping[6] = B_Paste; // BottomPasteMaskLayer -> B.Paste

        // 五、机械与结构�?
        m_layerIdMapping[10] = Edge_Cuts; // BoardOutLine -> Edge.Cuts
        m_layerIdMapping[15] = F_Fab;     // Mechanical -> F.Fab (顶层装配)
        m_layerIdMapping[99] = F_CrtYd;   // ComponentShapeLayer -> F.CrtYd (元件占位)
        m_layerIdMapping[100] = F_Fab;    // LeadShapeLayer -> F.Fab (引脚形状)
        m_layerIdMapping[101] = F_SilkS;  // ComponentPolarityLayer -> F.SilkS (极性标�?

        // 六、装配与文档�?
        m_layerIdMapping[13] = F_Fab;     // TopAssembly -> F.Fab
        m_layerIdMapping[14] = B_Fab;     // BottomAssembly -> B.Fab
        m_layerIdMapping[12] = Dwgs_User; // Document -> Dwgs.User

        // 七、特殊功能层
        m_layerIdMapping[9] = Dwgs_User;   // Ratlines -> Dwgs.User (飞线)
        m_layerIdMapping[11] = F_Cu;       // Multi-Layer -> F.Cu (通孔焊盘，实际在所有层)
        m_layerIdMapping[19] = Dwgs_User;  // 3DModel -> Dwgs.User (3D 模型引用)
        m_layerIdMapping[102] = Dwgs_User; // Hole -> Dwgs.User (非金属化�?
        m_layerIdMapping[103] = Cmts_User; // DRCError -> Cmts.User (DRC 错误)

        // 初始化嘉立创图层名称 -> KiCad 图层 ID 映射
        // 内层索引计算：In.(n-14).Cu
        // 例如：Inner1 -> In1.Cu, Inner2 -> In2.Cu...
        m_layerNameMapping["TopLayer"] = F_Cu;
        m_layerNameMapping["BottomLayer"] = B_Cu;
        for (int i = 1; i <= 32; i++)
        {
            // 使用清晰的公式：In1_Cu + (i - 1)
            // �?LCKiConverter �?In.(n-14).Cu 更直�?
            m_layerNameMapping[QString("Inner%1").arg(i)] = In1_Cu + (i - 1);
        }
        m_layerNameMapping["TopSilkLayer"] = F_SilkS;
        m_layerNameMapping["BottomSilkLayer"] = B_SilkS;
        m_layerNameMapping["TopSolderMaskLayer"] = F_Mask;
        m_layerNameMapping["BottomSolderMaskLayer"] = B_Mask;
        m_layerNameMapping["TopPasteMaskLayer"] = F_Paste;
        m_layerNameMapping["BottomPasteMaskLayer"] = B_Paste;
        m_layerNameMapping["BoardOutLine"] = Edge_Cuts;
        m_layerNameMapping["Mechanical"] = F_Fab;
        m_layerNameMapping["ComponentShapeLayer"] = F_CrtYd;
        m_layerNameMapping["LeadShapeLayer"] = F_Fab;
        m_layerNameMapping["ComponentPolarityLayer"] = F_SilkS;
        m_layerNameMapping["TopAssembly"] = F_Fab;
        m_layerNameMapping["BottomAssembly"] = B_Fab;
        m_layerNameMapping["Document"] = Dwgs_User;
        m_layerNameMapping["Ratlines"] = Dwgs_User;
        m_layerNameMapping["Multi-Layer"] = F_Cu;
        m_layerNameMapping["3DModel"] = Dwgs_User;
        m_layerNameMapping["Hole"] = Dwgs_User;
        m_layerNameMapping["DRCError"] = Cmts_User;

        qDebug() << "LayerMapper initialized with" << m_layerIdMapping.size() << "layer mappings";
    }

    int LayerMapper::mapToKiCadLayer(int easyedaLayerId) const
    {
        if (m_layerIdMapping.contains(easyedaLayerId))
        {
            return m_layerIdMapping.value(easyedaLayerId);
        }

        qWarning() << "Unknown EasyEDA layer ID:" << easyedaLayerId << ", mapping to Dwgs.User";
        return Dwgs_User; // 默认映射到用户绘图层
    }

    int LayerMapper::mapToKiCadLayer(const QString &easyedaLayerName) const
    {
        if (m_layerNameMapping.contains(easyedaLayerName))
        {
            return m_layerNameMapping.value(easyedaLayerName);
        }

        qWarning() << "Unknown EasyEDA layer name:" << easyedaLayerName << ", mapping to Dwgs.User";
        return Dwgs_User; // 默认映射到用户绘图层
    }

    QString LayerMapper::getKiCadLayerName(int kicadLayerId) const
    {
        if (m_kicadLayerNames.contains(kicadLayerId))
        {
            return m_kicadLayerNames.value(kicadLayerId);
        }

        return QString("Unknown(%1)").arg(kicadLayerId);
    }

    double LayerMapper::milToMm(double milValue)
    {
        return milValue * MIL_TO_MM;
    }

    double LayerMapper::mmToMil(double mmValue)
    {
        return mmValue * MM_TO_MIL;
    }

    bool LayerMapper::isSignalLayer(int kicadLayerId)
    {
        // 信号层：F.Cu (0), B.Cu (31), In1.Cu~In30.Cu (1-30)
        return (kicadLayerId == F_Cu || kicadLayerId == B_Cu ||
                (kicadLayerId >= In1_Cu && kicadLayerId <= In30_Cu));
    }

    bool LayerMapper::isSilkLayer(int kicadLayerId)
    {
        // 丝印层：F.SilkS (32), B.SilkS (33)
        return (kicadLayerId == F_SilkS || kicadLayerId == B_SilkS);
    }

    bool LayerMapper::isMaskLayer(int kicadLayerId)
    {
        // 阻焊层：F.Mask (34), B.Mask (35)
        return (kicadLayerId == F_Mask || kicadLayerId == B_Mask);
    }

    bool LayerMapper::isPasteLayer(int kicadLayerId)
    {
        // 助焊层：F.Paste (36), B.Paste (37)
        return (kicadLayerId == F_Paste || kicadLayerId == B_Paste);
    }

    bool LayerMapper::isMechanicalLayer(int kicadLayerId)
    {
        // 机械层：Edge.Cuts (44), F.CrtYd (45), B.CrtYd (46), F.Fab (47), B.Fab (48)
        //         Dwgs.User (49), Cmts.User (50), Eco1.User (51), Eco2.User (52), Margin (53)
        return (kicadLayerId >= Edge_Cuts && kicadLayerId <= Margin) ||
               (kicadLayerId >= User_1 && kicadLayerId <= User_9);
    }

    QString LayerMapper::getMappingDescription() const
    {
        QString desc = "=== 嘉立�?EDA -> KiCad 图层映射�?===\n\n";

        desc += "【一、电气信号层】\n";
        desc += "  嘉立�?ID 1 (TopLayer)           -> KiCad F.Cu (0)\n";
        desc += "  嘉立�?ID 2 (BottomLayer)       -> KiCad B.Cu (31)\n";
        desc += "  嘉立�?ID 21-52 (Inner1-32)    -> KiCad In1.Cu~In32.Cu (1-30)\n\n";

        desc += "【二、丝印层】\n";
        desc += "  嘉立�?ID 3 (TopSilkLayer)      -> KiCad F.SilkS (32)\n";
        desc += "  嘉立�?ID 4 (BottomSilkLayer)   -> KiCad B.SilkS (33)\n\n";

        desc += "【三、阻焊层】\n";
        desc += "  嘉立�?ID 7 (TopSolderMaskLayer) -> KiCad F.Mask (34)\n";
        desc += "  嘉立�?ID 8 (BottomSolderMaskLayer) -> KiCad B.Mask (35)\n\n";

        desc += "【四、助焊层】\n";
        desc += "  嘉立�?ID 5 (TopPasteMaskLayer) -> KiCad F.Paste (36)\n";
        desc += "  嘉立�?ID 6 (BottomPasteMaskLayer) -> KiCad B.Paste (37)\n\n";

        desc += "【五、机械与结构层】\n";
        desc += "  嘉立�?ID 10 (BoardOutLine)    -> KiCad Edge.Cuts (44)\n";
        desc += "  嘉立�?ID 15 (Mechanical)      -> KiCad F.Fab (47)\n";
        desc += "  嘉立�?ID 99 (ComponentShapeLayer) -> KiCad F.CrtYd (45)\n";
        desc += "  嘉立�?ID 100 (LeadShapeLayer)  -> KiCad F.Fab (47)\n";
        desc += "  嘉立�?ID 101 (ComponentPolarityLayer) -> KiCad F.SilkS (32)\n\n";

        desc += "【六、装配与文档层】\n";
        desc += "  嘉立�?ID 13 (TopAssembly)      -> KiCad F.Fab (47)\n";
        desc += "  嘉立�?ID 14 (BottomAssembly)   -> KiCad B.Fab (48)\n";
        desc += "  嘉立�?ID 12 (Document)         -> KiCad Dwgs.User (49)\n\n";

        desc += "【七、特殊功能层】\n";
        desc += "  嘉立�?ID 9 (Ratlines)          -> KiCad Dwgs.User (49)\n";
        desc += "  嘉立�?ID 11 (Multi-Layer)      -> KiCad F.Cu (0) [通孔焊盘]\n";
        desc += "  嘉立�?ID 19 (3DModel)          -> KiCad Dwgs.User (49)\n";
        desc += "  嘉立�?ID 102 (Hole)            -> KiCad Dwgs.User (49)\n";
        desc += "  嘉立�?ID 103 (DRCError)        -> KiCad Cmts.User (50)\n\n";

        desc += "【单位转换】\n";
        desc += "  1 mil = 0.0254 mm\n";
        desc += "  1 mm = 39.3701 mil\n\n";

        desc += "【注意事项】\n";
        desc += "  1. Multi-Layer 层的通孔焊盘需要在 KiCad 中设置为 'through' 类型\n";
        desc += "  2. 内层 (Inner1-32) 仅在多层板中有效�? 层板会自动忽略\n";
        desc += "  3. 阻焊层扩展�?(�?0.3) 需要在导出时转换为 mm\n";
        desc += "  4. 未知图层默认映射�?Dwgs.User 层\n";

        return desc;
    }

    // 元件外形层（边界层）判断
    // 元件外形用于布局避让，应映射�?F.CrtYd �?B.CrtYd
    bool LayerMapper::isCourtYardLayer(int easyedaLayerId)
    {
        // 根据 LCKiConverter，元件外形层 ID �?48
        // 注意：需要验证这�?ID 是否正确
        return easyedaLayerId == 48;
    }

    // 移植�?LCKiConverter: src/jlc/pro_footprint.ts getLayer()
    // 内层判断
    bool LayerMapper::isInnerLayer(int easyedaLayerId)
    {
        // 内层范围�?5-46 (对应 In1.Cu - In32.Cu)
        // 参�?LCKiConverter: n>=15 && n<=46
        return easyedaLayerId >= 15 && easyedaLayerId <= 46;
    }

    // 焊盘层判�?
    bool LayerMapper::isPadLayer(int easyedaLayerId)
    {
        // 焊盘层包括：
        // 1, 2 (SMD焊盘)
        // 11, 12 (通孔焊盘)
        // 15-46 (内层焊盘)
        return (easyedaLayerId == 1) ||
               (easyedaLayerId == 2) ||
               (easyedaLayerId == 11) ||
               (easyedaLayerId == 12) ||
               isInnerLayer(easyedaLayerId);
    }

    // 获取元件外形层对应的 KiCad 图层名称
    // 如果不是元件外形层，返回空字符串
    QString LayerMapper::getCourtYardLayerName(int easyedaLayerId)
    {
        if (isCourtYardLayer(easyedaLayerId))
        {
            return "F.CrtYd";
        }
        return QString();
    }

} // namespace EasyKiConverter
