package de.vw.mib.asl.api.navbap;
public interface INavLaneGuidanceDataNavBap {
    short getLaneDirection();byte getGuidanceInfo();byte getLaneDescription();byte getLaneMarkingLeft();byte getLaneMarkingRight();short getLaneType();byte[] getLaneSideStreets();
}
