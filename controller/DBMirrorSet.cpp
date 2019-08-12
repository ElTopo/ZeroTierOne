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

#include "DBMirrorSet.hpp"

namespace ZeroTier {

DBMirrorSet::DBMirrorSet(DB::ChangeListener *listener) :
	_listener(listener),
	_running(true)
{
	_syncCheckerThread = std::thread([this]() {
		for(;;) {
			for(int i=0;i<120;++i) { // 1 minute delay between checks
				if (!_running)
					return;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}

			std::vector< std::shared_ptr<DB> > dbs;
			{
				std::lock_guard<std::mutex> l(_dbs_l);
				if (_dbs.size() <= 1)
					continue; // no need to do this if there's only one DB, so skip the iteration
				dbs = _dbs;
			}

			for(auto db=dbs.begin();db!=dbs.end();++db) {
				(*db)->each([this,&dbs,&db](uint64_t networkId,const nlohmann::json &network,uint64_t memberId,const nlohmann::json &member) {
					try {
						if (network.is_object()) {
							if (memberId == 0) {
								for(auto db2=dbs.begin();db2!=dbs.end();++db2) {
									if (db->get() != db2->get()) {
										nlohmann::json nw2;
										if ((!(*db2)->get(networkId,nw2))||((nw2.is_object())&&(OSUtils::jsonInt(nw2["revision"],0) < OSUtils::jsonInt(network["revision"],0)))) {
											nw2 = network;
											(*db2)->save(nw2,false);
										}
									}
								}
							} else if (member.is_object()) {
								for(auto db2=dbs.begin();db2!=dbs.end();++db2) {
									if (db->get() != db2->get()) {
										nlohmann::json nw2,m2;
										if ((!(*db2)->get(networkId,nw2,memberId,m2))||((m2.is_object())&&(OSUtils::jsonInt(m2["revision"],0) < OSUtils::jsonInt(member["revision"],0)))) {
											m2 = member;
											(*db2)->save(m2,false);
										}
									}
								}
							}
						}
					} catch ( ... ) {} // skip entries that generate JSON errors
				});
			}
		}
	});
}

DBMirrorSet::~DBMirrorSet()
{
	_running = false;
	_syncCheckerThread.join();
}

bool DBMirrorSet::hasNetwork(const uint64_t networkId) const
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->hasNetwork(networkId))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network)) {
			return true;
		}
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,memberId,member))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,DB::NetworkSummaryInfo &info)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,memberId,member,info))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,members))
			return true;
	}
	return false;
}

void DBMirrorSet::networks(std::set<uint64_t> &networks)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->networks(networks);
	}
}

bool DBMirrorSet::waitForReady()
{
	bool r = false;
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		r |= (*d)->waitForReady();
	}
	return r;
}

bool DBMirrorSet::isReady()
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (!(*d)->isReady())
			return false;
	}
	return true;
}

bool DBMirrorSet::save(nlohmann::json &record,bool notifyListeners)
{
	std::vector< std::shared_ptr<DB> > dbs;
	{
		std::lock_guard<std::mutex> l(_dbs_l);
		dbs = _dbs;
	}
	if (notifyListeners) {
		for(auto d=dbs.begin();d!=dbs.end();++d) {
			if ((*d)->save(record,true))
				return true;
		}
		return false;
	} else {
		bool modified = false;
		for(auto d=dbs.begin();d!=dbs.end();++d) {
			modified |= (*d)->save(record,false);
		}
		return modified;
	}
}

void DBMirrorSet::eraseNetwork(const uint64_t networkId)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->eraseNetwork(networkId);
	}
}

void DBMirrorSet::eraseMember(const uint64_t networkId,const uint64_t memberId)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->eraseMember(networkId,memberId);
	}
}

void DBMirrorSet::nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->nodeIsOnline(networkId,memberId,physicalAddress);
	}
}

void DBMirrorSet::onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network)
{
	nlohmann::json record(network);
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (d->get() != db) {
			(*d)->save(record,false);
		}
	}
	_listener->onNetworkUpdate(this,networkId,network);
}

void DBMirrorSet::onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member)
{
	nlohmann::json record(member);
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (d->get() != db) {
			(*d)->save(record,false);
		}
	}
	_listener->onNetworkMemberUpdate(this,networkId,memberId,member);
}

void DBMirrorSet::onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId)
{
	_listener->onNetworkMemberDeauthorize(this,networkId,memberId);
}

} // namespace ZeroTier
