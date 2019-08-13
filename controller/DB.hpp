/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_CONTROLLER_DB_HPP
#define ZT_CONTROLLER_DB_HPP

//#define ZT_CONTROLLER_USE_LIBPQ

#include "../node/Constants.hpp"
#include "../node/Identity.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/BlockingQueue.hpp"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>
#include <set>

#include "../ext/json/json.hpp"

namespace ZeroTier
{

/**
 * Base class with common infrastructure for all controller DB implementations
 */
class DB
{
public:
	class ChangeListener
	{
	public:
		ChangeListener() {}
		virtual ~ChangeListener() {}
		virtual void onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network) {}
		virtual void onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member) {}
		virtual void onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId) {}
	};

	struct NetworkSummaryInfo
	{
		NetworkSummaryInfo() : authorizedMemberCount(0),totalMemberCount(0),mostRecentDeauthTime(0) {}
		std::vector<Address> activeBridges;
		std::vector<InetAddress> allocatedIps;
		unsigned long authorizedMemberCount;
		unsigned long totalMemberCount;
		int64_t mostRecentDeauthTime;
	};

	static void initNetwork(nlohmann::json &network);
	static void initMember(nlohmann::json &member);
	static void cleanNetwork(nlohmann::json &network);
	static void cleanMember(nlohmann::json &member);

	DB();
	virtual ~DB();

	virtual bool waitForReady() = 0;
	virtual bool isReady() = 0;

	inline bool hasNetwork(const uint64_t networkId) const
	{
		std::lock_guard<std::mutex> l(_networks_l);
		return (_networks.find(networkId) != _networks.end());
	}

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);

	void networks(std::set<uint64_t> &networks);

	template<typename F>
	inline void each(F f)
	{
		nlohmann::json nullJson;
		std::lock_guard<std::mutex> lck(_networks_l);
		for(auto nw=_networks.begin();nw!=_networks.end();++nw) {
			f(nw->first,nw->second->config,0,nullJson); // first provide network with 0 for member ID
			for(auto m=nw->second->members.begin();m!=nw->second->members.end();++m) {
				f(nw->first,nw->second->config,m->first,m->second);
			}
		}
	}

	virtual bool save(nlohmann::json &record,bool notifyListeners) = 0;

	virtual void eraseNetwork(const uint64_t networkId) = 0;
	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId) = 0;

	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress) = 0;

	inline void addListener(DB::ChangeListener *const listener)
	{
		std::lock_guard<std::mutex> l(_changeListeners_l);
		_changeListeners.push_back(listener);
	}

protected:
	static inline bool _compareRecords(const nlohmann::json &a,const nlohmann::json &b)
	{
		if (a.is_object() == b.is_object()) {
			if (a.is_object()) {
				if (a.size() != b.size())
					return false;
				auto amap = a.get<nlohmann::json::object_t>();
				auto bmap = b.get<nlohmann::json::object_t>();
				for(auto ai=amap.begin();ai!=amap.end();++ai) {
					if (ai->first != "revision") { // ignore revision, compare only non-revision-counter fields
						auto bi = bmap.find(ai->first);
						if ((bi == bmap.end())||(bi->second != ai->second))
							return false;
					}
				}
				return true;
			}
			return (a == b);
		}
		return false;
	}

	struct _Network
	{
		_Network() : mostRecentDeauthTime(0) {}
		nlohmann::json config;
		std::unordered_map<uint64_t,nlohmann::json> members;
		std::unordered_set<uint64_t> activeBridgeMembers;
		std::unordered_set<uint64_t> authorizedMembers;
		std::unordered_set<InetAddress,InetAddress::Hasher> allocatedIps;
		int64_t mostRecentDeauthTime;
		std::mutex lock;
	};

	void _memberChanged(nlohmann::json &old,nlohmann::json &memberConfig,bool notifyListeners);
	void _networkChanged(nlohmann::json &old,nlohmann::json &networkConfig,bool notifyListeners);
	void _fillSummaryInfo(const std::shared_ptr<_Network> &nw,NetworkSummaryInfo &info);

	std::vector<DB::ChangeListener *> _changeListeners;
	std::unordered_map< uint64_t,std::shared_ptr<_Network> > _networks;
	std::unordered_multimap< uint64_t,uint64_t > _networkByMember;
	mutable std::mutex _changeListeners_l;
	mutable std::mutex _networks_l;
};

} // namespace ZeroTier

#endif
