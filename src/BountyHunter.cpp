/* Copyright 
original script: Sydess
updated Script: Paranoia
released : privatedonut
Reworked : Micrah
Version : 3.3.5 
Bounty Hunter.
Updated : 02/01/2020
*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Creature.h"
#include "Define.h"
#include "GossipDef.h"
#include "Chat.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "CreatureAI.h"

#include <cstring>

#define SET_CURRENCY 0  //0代表金币，1代表荣誉点，2代表代币
#define TOKEN_ID 0 // token id

#if SET_CURRENCY == 0
#define BOUNTY_1 "我想悬赏100G。"
#define BOUNTY_2 "我想悬赏300G。"
#define BOUNTY_3 "我想悬赏500G。"
#define BOUNTY_4 "我想悬赏1000G。"
#define BOUNTY_5 "我想悬赏2000G。"
#define BOUNTY_6 "我想悬赏3000G。"
#define BOUNTY_7 "我想悬赏5000G。"
#define BOUNTY_8 "我想悬赏10000G。"
#endif
#if SET_CURRENCY == 1
#define BOUNTY_1 "我想悬赏100荣誉点。"
#define BOUNTY_2 "我想悬赏500荣誉点。"
#define BOUNTY_3 "我想悬赏1000荣誉点。"
#define BOUNTY_4 "我想悬赏2000荣誉点。"
#endif
#if SET_CURRENCY == 2
#define BOUNTY_1 "我想悬赏10T.C币。"
#define BOUNTY_2 "我想悬赏30T.C币。"
#define BOUNTY_3 "我想悬赏50T.C币。"

#endif

#define PLACE_BOUNTY "我想悬赏"
#define LIST_BOUNTY "列出当前的悬赏。"
#define NVM "别介意"
#define WIPE_BOUNTY "取消悬赏"




#if SET_CURRENCY != 2
//这些只是可视化的价格，如果您想更改真实的价格，请进一步编辑下面的sql
enum BountyPrice
{
	BOUNTY_PRICE_1 = 100,
	BOUNTY_PRICE_2 = 300,
	BOUNTY_PRICE_3 = 500,
	BOUNTY_PRICE_4 = 1000,
	BOUNTY_PRICE_5 = 2000,
	BOUNTY_PRICE_6 = 3000,
	BOUNTY_PRICE_7 = 5000,
	BOUNTY_PRICE_8 = 10000,
};
#else
enum BountyPrice
{
	BOUNTY_PRICE_1 = 100,
	BOUNTY_PRICE_2 = 500,
	BOUNTY_PRICE_3 = 1000,
	BOUNTY_PRICE_4 = 2000,
};
#endif

bool passChecks(Player * pPlayer, const char * name)
{ 

	Player * pBounty = sObjectAccessor->FindPlayerByName(name);
	WorldSession * m_session = pPlayer->GetSession();
	if(!pBounty)
	{
		m_session->SendNotification("玩家离线或不存在!");
		return false;
	}
	QueryResult result = CharacterDatabase.PQuery("SELECT * FROM bounties WHERE guid ='%u'", pBounty->GetGUID());
	if(result)
	{
		m_session->SendNotification("这个玩家已经得到了他们的赏金!");
		return false;
	}
	if(pPlayer->GetGUID() == pBounty->GetGUID())
	{
		m_session->SendNotification("你不能对自己设赏金!");
		return false;
	}
	return true;
}

void alertServer(const char * name, int msg)
{
	std::string message;
	if(msg == 1)
	{
		message = "已经有人悬赏了 ";
		message += name;
		message += ". 立即杀死他们以获得悬赏!";
	}
	else if(msg == 2)
	{
		message = "The bounty on ";
		message += name;
		message += " has been collected!";
	}
	sWorld->SendServerMessage(SERVER_MSG_STRING, message.c_str(), 0);
}


bool hasCurrency(Player * pPlayer, uint32 required, int currency)
{
	WorldSession *m_session = pPlayer->GetSession();
	switch(currency)
	{
		case 0: //gold
			{
			uint32 currentmoney = pPlayer->GetMoney();
			uint32 requiredmoney = (required * 10000);
			if(currentmoney < requiredmoney)
			{
				m_session->SendNotification("你没有足够的金币!");
				return false;
			}
			pPlayer->SetMoney(currentmoney - requiredmoney);
			break;
			}
		case 1: //honor
			{
			uint32 currenthonor = pPlayer->GetHonorPoints();
			if(currenthonor < required)
			{
				m_session->SendNotification("你没有足够的荣誉点!");
				return false;
			}
			pPlayer->SetHonorPoints(currenthonor - required);
			break;
			}
		case 2: //tokens
			{
			if(!pPlayer->HasItemCount(TOKEN_ID, required))
			{
				m_session->SendNotification("你没有足够的T.C币!");
				return false;
			}
			pPlayer->DestroyItemCount(TOKEN_ID, required, true, false);
			break;
			}

	}
	return true;
}

void flagPlayer(const char * name)
{
	Player * pBounty = sObjectAccessor->FindPlayerByName(name);
	pBounty->SetPvP(true);
	pBounty->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
}

class BountyHunter : public CreatureScript
{
	public:
		BountyHunter() : CreatureScript("BountyHunter"){}
		bool OnGossipHello(Player * Player, Creature * Creature)
		{
			Player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, PLACE_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+1);
			Player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, LIST_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+2);
			Player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, NVM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+3);
			if ( Player->IsGameMaster() )
				Player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, WIPE_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+4);

			Player->PlayerTalkClass->SendGossipMenu(907, Creature->GetGUID());
			return true;
		}

		bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
		{
			pPlayer->PlayerTalkClass->ClearMenus();
			switch(uiAction)
			{
				case GOSSIP_ACTION_INFO_DEF+1:
				{
					pPlayer->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_BATTLE, BOUNTY_1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+5, "", 0, true);
					pPlayer->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_BATTLE, BOUNTY_2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+6, "", 0, true);
					pPlayer->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_BATTLE, BOUNTY_3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+7, "", 0, true);
					pPlayer->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_BATTLE, BOUNTY_4, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+8, "", 0, true);
					pPlayer->PlayerTalkClass->SendGossipMenu(365, pCreature->GetGUID());
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+2:
				{
					QueryResult Bounties = CharacterDatabase.PQuery("SELECT * FROM bounties");
					
					if(!Bounties)
					{
						pPlayer->PlayerTalkClass->SendCloseGossip();
						return false;
					}
#if SET_CURRENCY == 0
					if(	Bounties->GetRowCount() > 1)
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
							Field * names = name->Fetch();
							option = names[0].GetString();
							option +=" ";
							option += fields[1].GetString();
							option += " 金币";
							pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
						Field * names = name->Fetch();
						option = names[0].GetString();
						option +=" ";
						option += fields[1].GetString();
						option += " 金币";
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
#if SET_CURRENCY == 1
					if(	Bounties->GetRowCount() > 1)
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
							Field * names = name->Fetch();
							option = names[0].GetString();
							option +=" ";
							option += fields[1].GetString();
							option += " 荣誉点";
							pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
						Field * names = name->Fetch();
						option = names[0].GetString();
						option +=" ";
						option += fields[1].GetString();
						option += " 荣誉点";
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
#if SET_CURRENCY == 2
					if(	Bounties->GetRowCount() > 1)
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
							Field * names = name->Fetch();
							option = names[0].GetString();
							option +=" ";
							option += fields[1].GetString();
							option += " T.C币";
							pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid='%u'", fields[0].GetUInt64());
						Field * names = name->Fetch();
						option = names[0].GetString();
						option +=" ";
						option += fields[1].GetString();
						option += " T.C币";
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
					pPlayer->PlayerTalkClass->SendGossipMenu(878, pCreature->GetGUID());
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+3:
				{
					pPlayer->PlayerTalkClass->SendCloseGossip();
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+4:
				{
					CharacterDatabase.PExecute("TRUNCATE TABLE bounties");
					pPlayer->PlayerTalkClass->SendCloseGossip();
					break;
				}
			}
			return true;
		}

		bool OnGossipSelectCode(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char * code)
		{
			pPlayer->PlayerTalkClass->ClearMenus();
			if ( uiSender == GOSSIP_SENDER_MAIN )
			{
				if(islower(code[0]))
					toupper(code[0]);

				if(passChecks(pPlayer, code))
				{
					Player * pBounty = sObjectAccessor->FindPlayerByName(code);
					switch (uiAction)
					{
						case GOSSIP_ACTION_INFO_DEF+5:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_1, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u','20', '1')", pBounty->GetGUID());
								#else
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u','1', '1')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								pPlayer->PlayerTalkClass->SendCloseGossip();
							}
							break;
						}
							
						case GOSSIP_ACTION_INFO_DEF+6:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_2, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '40', '2')", pBounty->GetGUID());
								#else
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '3', '2')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								pPlayer->PlayerTalkClass->SendCloseGossip();
							}
							break;
						}
						case GOSSIP_ACTION_INFO_DEF+7:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_3, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '100', '3')", pBounty->GetGUID());
								#else
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '5', '3')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								pPlayer->PlayerTalkClass->SendCloseGossip();
							}
							break;
						}
						case GOSSIP_ACTION_INFO_DEF+8:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_4, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '200', '4')", pBounty->GetGUID());
								#else
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES('%u', '10', '3')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								pPlayer->PlayerTalkClass->SendCloseGossip();
							}
							break;
						}
						

					}
				}
				else
					pPlayer->PlayerTalkClass->SendCloseGossip();
			}
			return true;
		}
};

class BountyhunterAnnounce : public PlayerScript
{

public:

    BountyhunterAnnounce() : PlayerScript("BountyhunterAnnounce") {}

    void OnLogin(Player* player)
    {
        // Announce Module
        if (sConfigMgr->GetBoolDefault("BountyhunterAnnounce.Enable", true))
        {
                ChatHandler(player->GetSession()).SendSysMessage("服务器已启用 |cff4CFF00赏金猎人 |模块。");
         }
    }
};

class BountyKills : public PlayerScript
{
	public:
		BountyKills() : PlayerScript("BountyKills"){}
		
		void OnPVPKill(Player * Killer, Player * Bounty)
    
    {
			        // If enabled...
        if (sConfigMgr->GetBoolDefault("BountyHunter.Enable", true)) 
		{
			if(Killer->GetGUID() == Bounty->GetGUID())
				return;

			QueryResult result = CharacterDatabase.PQuery("SELECT * FROM bounties WHERE guid='%u'", Bounty->GetGUID());
			if(!result)
				return;

			Field * fields = result->Fetch();
			#if SET_CURRENCY == 0
				switch(fields[2].GetUInt64())
				{
				case 1:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_1 * 10000));
					break;
				case 2:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_2 * 10000));
					break;
				case 3:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_3 * 10000));
					break;
				case 4:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_4 * 10000));
					break;
				case 5:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_5 * 10000));
					break;
				case 6:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_6 * 10000));
					break;
				case 7:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_7 * 10000));
					break;
				case 8:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_8 * 10000));
					break;
				}
			#endif

			#if SET_CURRENCY == 1
				switch(fields[2].GetUInt64())
				{
				case 1:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_1));
					break;
				case 2:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_2));
					break;
				case 3:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_3));
					break;
				case 4:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_4));
					break;
				case 5:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_5));
					break;
				case 6:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_6));
					break;
				case 7:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_7));
					break;	
				case 8:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_8));
					break;
				}
			#endif

			#if SET_CURRENCY == 2
				switch(fields[2].GetUInt64())
				{
				case 1:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_1);
					break;
				case 2:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_2);
					break;
				case 3:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_3);
					break;

				}
			#endif
			CharacterDatabase.PExecute("DELETE FROM bounties WHERE guid='%u'", Bounty->GetGUID());
			alertServer(Bounty->GetName().c_str(), 2);
        }

	}
};

class BountyHunterConfig : public WorldScript
{
public:
    BountyHunterConfig() : WorldScript("BountyHunterConfig") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            std::string conf_path = _CONF_DIR;
            std::string cfg_file = conf_path + "/bountyhunter.conf";

            std::string cfg_def_file = cfg_file + ".dist";
            sConfigMgr->LoadMore(cfg_def_file.c_str());
            sConfigMgr->LoadMore(cfg_file.c_str());
        }
    }
};

void AddBountyHunterScripts()
{
        new BountyHunterConfig();
        new BountyhunterAnnounce();
	new BountyHunter();
	new BountyKills();
}
