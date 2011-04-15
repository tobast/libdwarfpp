#include <deque>
#include <utility>
#include <boost/make_shared.hpp>

#include "spec_adt.hpp"
#include "adt.hpp"
#include "attr.hpp"

namespace dwarf
{
	namespace spec
    {
/* from spec::basic_die */
        boost::shared_ptr<basic_die> basic_die::get_this()
        { return this->get_ds()[this->get_offset()]; }
        boost::shared_ptr<basic_die> basic_die::get_this() const
        { return this->get_ds()[this->get_offset()]; }

        boost::optional<std::vector<std::string> >
        basic_die::ident_path_from_root() const
        {
            if (get_offset() == 0) return std::vector<std::string>(); // empty
            else if (get_name())
            {
	            boost::optional<std::vector<std::string> > built 
                 = const_cast<basic_die*>(this)->get_parent()->ident_path_from_root();
                if (!built) return 0;
                else
                {
	                (*built).push_back(*get_name());
    	            return built;
                }
            }
            else return 0;
        }

        boost::optional<std::vector<std::string> >
        basic_die::ident_path_from_cu() const
        {
            if (get_offset() == 0) return 0; // error
            if (get_tag() == DW_TAG_compile_unit) return std::vector<std::string>(); // empty
            else if (get_name()) // recursive case
            {
                // try to build our parent's path
	            boost::optional<std::vector<std::string> >
                    built = const_cast<basic_die*>(this)->get_parent()->ident_path_from_cu();
                if (!built) return 0;
                else // success, so just add our own name to the path
                {
	                (*built).push_back(*get_name());
    	            return built;
                }
            }
            else return 0; // error -- we have no name
        }

		std::vector< boost::optional<std::string> >
		basic_die::opt_ident_path_from_root() const
		{
			if (get_offset() == 0) return std::vector<boost::optional<std::string> >(); // empty
			else 
			{
				// assert that we have a parent but it's not us
				assert(this->get_parent() && this->get_parent()->get_offset() != get_offset());
				std::vector< boost::optional<std::string> > built 
				 = const_cast<basic_die*>(this)->get_parent()->opt_ident_path_from_root();
				built.push_back(get_name());
				return built;
			}
		}

		std::vector < boost::optional<std::string> >
		basic_die::opt_ident_path_from_cu() const
		{
			if (get_offset() == 0) return std::vector < boost::optional<std::string> >(); // error
			if (get_tag() == DW_TAG_compile_unit)
			{
				return std::vector<boost::optional<std::string> >(); // empty
			}
			else // recursive case
			{
				// assert that we have a parent but it's not us
				
				// assertion pre-failure debugging aid
				if (!(this->get_parent() && this->get_parent()->get_offset() != get_offset()))
				{
					std::cerr << *this;
				}
				assert(this->get_parent() && this->get_parent()->get_offset() != get_offset());
				// try to build our parent's path
				std::vector< boost::optional< std::string> >
				built = const_cast<basic_die*>(this)->get_parent()->opt_ident_path_from_cu();
				built.push_back(get_name());
				return built;
			}
		}

        boost::shared_ptr<spec::basic_die> 
        basic_die::nearest_enclosing(Dwarf_Half tag) 
        {
            /*if (get_tag() == 0 || get_offset() == 0UL) return NULL_SHARED_PTR(spec::basic_die);
            else if (get_parent()->get_tag() == tag) 
            {
				return get_parent();
            }
            else return get_parent()->nearest_enclosing(tag);*/
            
            // instead use more efficient (and hopefully correct!) iterative approach
            // -- this avoids recomputing the path for each recursive step
            auto my_iter = this->get_ds().find(this->get_offset()); // computes path
            assert(my_iter.base().path_from_root.size() >= 1);
            for (auto path_iter = ++my_iter.base().path_from_root.rbegin(); 
            	path_iter != my_iter.base().path_from_root.rend();
                path_iter++)
            {
            	if (this->get_ds()[path_iter->off]->get_tag() == tag)
                {
                	return this->get_ds()[path_iter->off];
                }
			}
            return boost::shared_ptr<spec::basic_die>();
        }

        boost::shared_ptr<compile_unit_die> 
        basic_die::enclosing_compile_unit()
        {
        	// HACK: special case: compile units enclose themselves (others don't)
            // -- see with_runtime_location_die::contains_addr for motivation
            return boost::dynamic_pointer_cast<compile_unit_die>(
            	this->get_tag() == DW_TAG_compile_unit ?
                	this->get_this()
            	:	nearest_enclosing(DW_TAG_compile_unit));
		}
            
        boost::shared_ptr<spec::basic_die>
        basic_die::find_sibling_ancestor_of(boost::shared_ptr<spec::basic_die> p_d) 
        {
            // search upward from the argument die to find a sibling of us
            if (p_d.get() == dynamic_cast<spec::basic_die*>(this)) return p_d;
            else if (p_d->get_offset() == 0UL) return NULL_SHARED_PTR(spec::basic_die); // reached the top without finding anything
            else if (this->get_offset() == 0UL) return NULL_SHARED_PTR(spec::basic_die); // we have no siblings
            else if (p_d->get_parent() == this->get_parent()) // we are siblings
            {
                return p_d;
            }
            else return find_sibling_ancestor_of(p_d->get_parent()); // recursive case
        }
        
        std::ostream& operator<<(std::ostream& o, const ::dwarf::spec::basic_die& d)
        {
			o 	<< "DIE, child of 0x" 
            	<< std::hex << ((d.get_parent()) 
                				? d.get_parent()->get_offset()
			                	: 0UL)
                    << std::dec 
				<< ", tag: " << d.get_ds().get_spec().tag_lookup(d.get_tag()) 
				<< ", offset: 0x" << std::hex << d.get_offset() << std::dec 
				<< ", name: "; 
            auto attrs = const_cast<basic_die&>(d).get_attrs();
            if (attrs.find(DW_AT_name) != attrs.end()) o << attrs.find(DW_AT_name)->second; 
            else o << "(no name)"; 
            o << std::endl;

			for (std::map<Dwarf_Half, encap::attribute_value>::const_iterator p 
					= attrs.begin();
				p != attrs.end(); p++)
			{
				o << "\t";
				o << "Attribute " << d.get_ds().get_spec().attr_lookup(p->first) << ", value: ";
				p->second.print_as(o, d.get_ds().get_spec().get_interp(
                	p->first, p->second.orig_form));
				o << std::endl;	
			}			

			return o;
        }
        
/* from spec::with_runtime_location_die */
		boost::optional<Dwarf_Off> // returns *offset within the element*
        with_runtime_location_die::contains_addr(Dwarf_Addr file_relative_address,
        	sym_binding_t (*sym_resolve)(const std::string& sym, void *arg), 
			void *arg /* = 0 */) const
        {
        	// FIXME: get rid of the const_casts
            auto nonconst_this = const_cast<with_runtime_location_die *>(this);
        	auto attrs = nonconst_this->get_attrs();

        	// HACK: if we're a local variable, return false. This function
            // only deals with static storage. Mostly the restriction is covered
            // by the fact that only certain tags are with_runtime_location_dies,
            // but both locals and globals show up with DW_TAG_variable.
            if (this->get_tag() == DW_TAG_variable &&
            	!dynamic_cast<const variable_die *>(this)->has_static_storage())
                return boost::optional<Dwarf_Off>();

            auto found_low_pc = attrs.find(DW_AT_low_pc);
            auto found_high_pc = attrs.find(DW_AT_high_pc);
           	auto found_ranges = attrs.find(DW_AT_ranges);
           	auto found_location = attrs.find(DW_AT_location);
            auto found_mips_linkage_name = attrs.find(DW_AT_MIPS_linkage_name); // HACK: MIPS should...
            auto found_linkage_name = attrs.find(DW_AT_linkage_name); // ... be in a non-default spec
            if (found_low_pc != attrs.end()
            	&& found_high_pc != attrs.end())
            {
            	//std::cerr << "DIE at 0x" << std::hex << this->get_offset()
                //	<< " has low/high PC " << found_low_pc->second.get_address() << ", "
                //    << found_high_pc->second.get_address() << std::endl;
            	if (file_relative_address >= found_low_pc->second.get_address()
                	&& file_relative_address < found_high_pc->second.get_address())
                {
                	return file_relative_address - found_low_pc->second.get_address();
                }
                else return boost::optional<Dwarf_Off>();
			}
            else if (found_ranges != attrs.end())
            {
            	auto rangelist = found_ranges->second.get_rangelist();
                //std::cerr << "DIE at 0x" << std::hex << this->get_offset()
                //	<< " has rangelist " << rangelist << std::endl;
                auto nonconst_this = const_cast<with_runtime_location_die *>(this);
                // rangelist::find_addr() requires a dieset-relative (i.e. file-relative) address
                /*assert(nonconst_this->enclosing_compile_unit()->get_low_pc());*/
             	auto range_found = rangelist.find_addr(
                 	file_relative_address /*- 
                     *(nonconst_this->enclosing_compile_unit()->get_low_pc())*/);
                 if (!range_found) return boost::optional<Dwarf_Off>();
                 else 
                 {
                 	return range_found->first;
                 }
            }
            else if (found_location != attrs.end())
            {
            	/* Location lists can be vaddr-dependent, where vaddr is the 
                 * offset of the current PC within the containing subprogram.
                 * However, this is only for parameters and locals, and even
                 * then, they are usually relative to a DW_AT_frame_base, and
                 * only that is vaddr-dependent. In any case, because we only
                 * deal with variables and subprograms and 
                 * lexical blocks and inlined subprogram
                 * instances here, I don't *think* we need to worry about this. */
                
                /* FIXME: if we *do* need to worry about that, work out
                 * whether lexical block DIEs can ever have
                 * vaddr-dependent location lists, and if so, whether
                 * they use subprogram-relative or block-relative vaddrs. */
                
            	/* Here we need a location and a size, which may come
                 * directly, or from the type. First calculate the byte size (easier). */
                Dwarf_Unsigned byte_size;
                //std::cerr << "DIE at 0x" << std::hex << this->get_offset()
                //	<< " has location list " << found_location->second.get_loclist() << std::endl;
                auto found_byte_size = attrs.find(DW_AT_byte_size);
                if (found_byte_size != attrs.end())
                {
                	byte_size = found_byte_size->second.get_unsigned();
                }
                else
                {	
                	/* Look for the type. "Type" means something different
                     * for a subprogram, which should be covered by the
                     * high_pc/low_pc and ranges cases, so assert that
	                 * we don't have one of those. */
                    assert(this->get_tag() != DW_TAG_subprogram);
                    auto found_type = attrs.find(DW_AT_type);
                    if (found_type == attrs.end()) return boost::optional<Dwarf_Off>();
                    else
                    {
                    	auto calculated_byte_size = boost::dynamic_pointer_cast<spec::type_die>(
                    		this->get_ds()[found_type->second.get_ref().off])->calculate_byte_size();
                        assert(calculated_byte_size);
                        byte_size = *calculated_byte_size;
                    }
				}
                
                auto loclist = found_location->second.get_loclist();
                auto expr_pieces = loclist.loc_for_vaddr(0).pieces();
                Dwarf_Off current_offset_within_object = 0UL;
                for (auto i = expr_pieces.begin(); i != expr_pieces.end(); i++)
                {
                	/* Evaluate this pieces and see if it spans the address
                     * we're interested in. */
	                Dwarf_Unsigned location = dwarf::lib::evaluator(i->first,
	                    this->get_spec()).tos();
                    if (file_relative_address >= location
                    	&& file_relative_address < location + i->second)
                        // match
                        return current_offset_within_object + (file_relative_address - location);
                    else current_offset_within_object += i->second;
                }
            }
            else if (sym_resolve &&
            	(found_mips_linkage_name != attrs.end()
            	|| found_linkage_name != attrs.end()))
            {
            	std::string linkage_name;
                
            	// prefer the DWARF 4 attribute to the MIPS/GNU/... extension
                if (found_linkage_name != attrs.end()) linkage_name 
                 = found_linkage_name->second.get_string();
                else 
                { 
                	assert(found_mips_linkage_name != attrs.end());
                	linkage_name = found_mips_linkage_name->second.get_string();
                }
                
                std::cerr << "DIE at 0x" << std::hex << this->get_offset()
                	<< " has linkage name " << linkage_name << std::endl;
                
                sym_binding_t binding;
                try
                {
		            binding = sym_resolve(linkage_name, arg);
                    if (file_relative_address >= binding.file_relative_start_addr
                	    && file_relative_address < binding.file_relative_start_addr + binding.size)
                    {
                    	// slight HACK: assume objects located only by DW_AT_linkage_address...
                        // ... are contiguous in memory
                	    return boost::optional<Dwarf_Off>(
                        	file_relative_address -
                            	binding.file_relative_start_addr);
                    }
                    // else fall through to final return statement
                }
                catch (lib::No_entry)
                {
                	std::cerr << "Warning: couldn't resolve linkage name " << linkage_name
                    	<< " for DIE " << *this << std::endl;
                }
            }
            return boost::optional<Dwarf_Off>();
        }
/* helpers */        
        static encap::loclist loclist_from_pc_values(Dwarf_Addr low_pc, Dwarf_Addr high_pc);
        static encap::loclist loclist_from_pc_values(Dwarf_Addr low_pc, Dwarf_Addr high_pc)
        {
            Dwarf_Unsigned opcodes[] 
            = { DW_OP_constu, low_pc, 
                DW_OP_piece, high_pc - low_pc };
            encap::loclist list(encap::loc_expr(opcodes, 0, std::numeric_limits<Dwarf_Addr>::max())); 
            return list;
        }
        static encap::loclist loclist_from_pc_values(Dwarf_Addr low_pc);
        static encap::loclist loclist_from_pc_values(Dwarf_Addr low_pc)
        {
            Dwarf_Unsigned opcodes[] 
            = { DW_OP_constu, low_pc };
            encap::loclist list(encap::loc_expr(opcodes, 0, std::numeric_limits<Dwarf_Addr>::max())); 
            return list;
        }
		encap::loclist with_runtime_location_die::get_runtime_location() const
        {
        	auto attrs = const_cast<with_runtime_location_die *>(this)->get_attrs();
            if (attrs.find(DW_AT_location) != attrs.end())
            {
            	return attrs.find(DW_AT_location)->second.get_loclist();
            }
            else
        	/* This is a dieset-relative address. */
            if (attrs.find(DW_AT_low_pc) != attrs.end() 
            	&& attrs.find(DW_AT_high_pc) != attrs.end())
            {
            	return loclist_from_pc_values(attrs.find(DW_AT_low_pc)->second.get_address().addr,
                	attrs.find(DW_AT_high_pc)->second.get_address().addr);
            }
            else
            {
            	assert(attrs.find(DW_AT_low_pc) != attrs.end());
        	    return loclist_from_pc_values(
                	attrs.find(DW_AT_low_pc)->second.get_address().addr);
            }
        } 
/* from spec::subprogram_die */
		boost::optional< std::pair<Dwarf_Off, boost::shared_ptr<with_stack_location_die> > >
        subprogram_die::contains_addr_as_frame_local_or_argument( 
            	    Dwarf_Addr absolute_addr, 
                    Dwarf_Off dieset_relative_ip, 
                    Dwarf_Signed *out_frame_base,
                    dwarf::lib::regs *p_regs/* = 0*/) const
        {
        	auto nonconst_this = const_cast<subprogram_die *>(this);
        	assert(this->get_frame_base());
        	/* First we calculate our frame base address. */
            auto frame_base_addr = dwarf::lib::evaluator(
                *this->get_frame_base(),
                dieset_relative_ip, // this is the vaddr which selects a loclist element
                this->get_ds().get_spec(),
                p_regs).tos();
            if (out_frame_base) *out_frame_base = frame_base_addr;
            
            if (!this->first_child_offset()) return 0;
         	/* Now we walk children
             * (not just immediate children, because more might hide under lexical_blocks), 
             * looking for with_stack_location_dies, and 
             * call contains_addr on what we find. */
            abstract_dieset::bfs_policy bfs_state;
            abstract_dieset::iterator start_iterator
             = this->get_first_child()->iterator_here(bfs_state);
            //std::cerr << "Exploring stack-located children of " << *this << std::endl;
            unsigned initial_depth = start_iterator.base().path_from_root.size();
            for (auto i_bfs = start_iterator;
            		i_bfs.base().path_from_root.size() >= initial_depth;
                    i_bfs++)
            {
            	//std::cerr << "Considering whether DIE has stack location: " << **i_bfs << std::endl;
            	auto with_stack_loc = boost::dynamic_pointer_cast<spec::with_stack_location_die>(
                	*i_bfs);
                if (!with_stack_loc) continue;
                
                boost::optional<Dwarf_Off> result = with_stack_loc->contains_addr(absolute_addr,
                	frame_base_addr,
                    dieset_relative_ip,
                    p_regs);
                if (result) return std::make_pair(*result, with_stack_loc);
            }
            return boost::optional< std::pair<Dwarf_Off, boost::shared_ptr<with_stack_location_die> > >();
        }
        bool subprogram_die::is_variadic() const
        {
    	    try
            {
    	        for (auto i_child = this->get_first_child(); i_child;  // term'd by exception
            	    i_child = i_child->get_next_sibling())
                {
                    if (i_child->get_tag() == DW_TAG_unspecified_parameters)
                    {
            	        return true;
                    }
                }
        	}
            catch (No_entry) {}
            return false;
        }

/* from spec::with_stack_location_die */
		boost::optional<Dwarf_Off> with_stack_location_die::contains_addr(
                    Dwarf_Addr absolute_addr,
                    Dwarf_Signed frame_base_addr,
                    Dwarf_Off dieset_relative_ip,
                    dwarf::lib::regs *p_regs) const
        {
        	auto attrs = const_cast<with_stack_location_die *>(this)->get_attrs();
            assert(attrs.find(DW_AT_type) != attrs.end());
            assert(attrs.find(DW_AT_location) != attrs.end());
            auto size = *(attrs.find(DW_AT_type)->second.get_refdie_is_type()->calculate_byte_size());
            auto base_addr = dwarf::lib::evaluator(
                attrs.find(DW_AT_location)->second.get_loclist(),
                dieset_relative_ip,
                this->get_ds().get_spec(),
                p_regs,
                frame_base_addr).tos();
            if (absolute_addr >= base_addr
            &&  absolute_addr < base_addr + size)
            {
 				return absolute_addr - base_addr;
            }
            return boost::optional<Dwarf_Off>();
        }

/* from spec::with_named_children_die */
        boost::shared_ptr<spec::basic_die>
        with_named_children_die::named_child(const std::string& name) 
        { 
			try
            {
            	for (auto current = this->get_first_child();
                		; // terminates by exception
                        current = current->get_next_sibling())
                {
                	if (current->get_name() 
                    	&& *current->get_name() == name) return current;
                }
            }
            catch (No_entry) { return NULL_SHARED_PTR(spec::basic_die); }
        }

        boost::shared_ptr<spec::basic_die> 
        with_named_children_die::resolve(const std::string& name) 
        {
            std::vector<std::string> multipart_name;
            multipart_name.push_back(name);
            return resolve(multipart_name.begin(), multipart_name.end());
        }

        boost::shared_ptr<spec::basic_die> 
        with_named_children_die::scoped_resolve(const std::string& name) 
        {
            std::vector<std::string> multipart_name;
            multipart_name.push_back(name);
            return scoped_resolve(multipart_name.begin(), multipart_name.end());
        }
/* from spec::compile_unit_die */
		boost::optional<Dwarf_Unsigned> compile_unit_die::implicit_array_base() const
        {
        	switch(this->get_language())
            {
            	/* See DWARF 3 sec. 5.12! */
            	case DW_LANG_C:
                case DW_LANG_C89:
                case DW_LANG_C_plus_plus:
                case DW_LANG_C99:
                	return boost::optional<Dwarf_Unsigned>(0UL);
                case DW_LANG_Fortran77:
                case DW_LANG_Fortran90:
                case DW_LANG_Fortran95:
                	return boost::optional<Dwarf_Unsigned>(1UL);
                default:
                	return boost::optional<Dwarf_Unsigned>();
            }
        }
/* from spec::type_die */
        boost::optional<Dwarf_Unsigned> type_die::calculate_byte_size() const
        {
        	//return boost::optional<Dwarf_Unsigned>();
            if (this->get_byte_size()) return *this->get_byte_size();
            else return boost::optional<Dwarf_Unsigned>();
		}
        boost::shared_ptr<type_die> type_die::get_concrete_type() const
        {
        	// by default, our concrete self is our self
        	return boost::dynamic_pointer_cast<type_die>(
            	const_cast<type_die *>(this)->get_ds()[this->get_offset()]);
        } 
        boost::shared_ptr<type_die> type_die::get_concrete_type()
        {
        	return const_cast<const type_die *>(this)->get_concrete_type();
        }
/* from spec::type_chain_die */
        boost::optional<Dwarf_Unsigned> type_chain_die::calculate_byte_size() const
        {
        	// Size of a type_chain is always the size of its concrete type
            // which is *not* to be confused with its pointed-to type!
        	if (this->get_concrete_type()) return this->get_concrete_type()->calculate_byte_size();
            else return boost::optional<Dwarf_Unsigned>();
		}
        boost::shared_ptr<type_die> type_chain_die::get_concrete_type() const
        {
        	// pointer and reference *must* override us -- they do not follow chain
        	assert(this->get_tag() != DW_TAG_pointer_type
            	&& this->get_tag() != DW_TAG_reference_type);

            if (!this->get_type()) 
            {
            	return //boost::dynamic_pointer_cast<type_die>(get_this()); // broken chain
					boost::shared_ptr<type_die>();
            }
            else return (*const_cast<type_chain_die*>(this)->get_type())->get_concrete_type();
        }
/* from spec::pointer_type_die */  
        boost::shared_ptr<type_die> pointer_type_die::get_concrete_type() const 
        {
        	return boost::dynamic_pointer_cast<pointer_type_die>(get_this()); 
        }
        boost::optional<Dwarf_Unsigned> pointer_type_die::calculate_byte_size() const 
        {
        	assert(this->get_byte_size()); return this->get_byte_size();
        }
/* from spec::reference_type_die */  
        boost::shared_ptr<type_die> reference_type_die::get_concrete_type() const 
        {
        	return boost::dynamic_pointer_cast<reference_type_die>(get_this()); 
        }
        boost::optional<Dwarf_Unsigned> reference_type_die::calculate_byte_size() const 
        {
        	assert(this->get_byte_size()); return this->get_byte_size();
        }
/* from spec::array_type_die */
		boost::optional<Dwarf_Unsigned> array_type_die::element_count() const
        {
        	assert(this->get_type());
            boost::optional<Dwarf_Unsigned> count;
            
			try
            {
				for (auto child = this->get_first_child(); ; child = child->get_next_sibling())
                {
                	if (child->get_tag() == DW_TAG_subrange_type)
                    {
            	        auto subrange = boost::dynamic_pointer_cast<subrange_type_die>(child);
                        if (subrange->get_count()) 
                        {
                        	count = *subrange->get_count();
                            break;
                        }
                        else
                        {
                	        Dwarf_Unsigned lower_bound;
                            if (subrange->get_lower_bound()) lower_bound = *subrange->get_lower_bound();
                            else if (subrange->enclosing_compile_unit()->implicit_array_base())
                            {
                            	lower_bound = *subrange->enclosing_compile_unit()->implicit_array_base();
                            }
                            else break; // give up
                            if (!subrange->get_upper_bound()) break; // give up
                            
                            boost::optional<Dwarf_Unsigned> upper_bound_optional = 
                            	static_cast<boost::optional<Dwarf_Unsigned> >(
                                	subrange->get_upper_bound());

                            assert(*subrange->get_upper_bound() < 10000000); // detects most garbage
                            Dwarf_Unsigned upper_bound = *subrange->get_upper_bound();
                            count = upper_bound - lower_bound + 1;
                        }
                    } // end if it's a subrange type
                    /* FIXME: also handle enumeration types, because
                     * DWARF allows array indices to be specified by
                     * an enumeration as well as a subrange. */
                } // end for
            } // end try
            catch (No_entry) {} // termination of loop
            
            return count;
    	}

        boost::optional<Dwarf_Unsigned> array_type_die::calculate_byte_size() const
        {
        	assert(this->get_type());
            boost::optional<Dwarf_Unsigned> count = this->element_count();
            boost::optional<Dwarf_Unsigned> calculated_byte_size
             = (*this->get_type())->calculate_byte_size();
            if (count && calculated_byte_size) return *count * *calculated_byte_size;
            else return boost::optional<Dwarf_Unsigned>();
		}
/* from spec::variable_die */        
		bool variable_die::has_static_storage() const
        {
			auto nonconst_this = const_cast<variable_die *>(this);
            if (nonconst_this->nearest_enclosing(DW_TAG_subprogram))
            {
            	// we're either a local or a static -- skip if local
                auto attrs = nonconst_this->get_attrs();
            	if (attrs.find(DW_AT_location) != attrs.end())
                {
                	// HACK: only way to work out whether it's static
                    // is to test for frame-relative addressing in the location
                    // --- *and* since we can't rely on the compiler to generate
                    // DW_OP_fbreg for every frame-relative variable (since it
                    // might just use %ebp or %esp directly), rule out any
                    // register-relative location whatsoever. FIXME: this might
                    // break some code on segmented architectures, where even
                    // static storage is recorded in DWARF using 
                    // register-relative addressing....
                    auto loclist = attrs.find(DW_AT_location)->second.get_loclist();
                    for (auto i_loc_expr = loclist.begin(); i_loc_expr != loclist.end(); i_loc_expr++)
                    {
                    	for (auto i_instr = i_loc_expr->begin(); 
                        	i_instr != i_loc_expr->end();
                            i_instr++)
                        {
                        	if (this->get_spec().op_reads_register(i_instr->lr_atom)) return false;
                        }
                    }
                }
            }
            return true;
		}
/* from spec::member_die */        
		boost::optional<Dwarf_Unsigned> 
        member_die::byte_offset_in_enclosing_type() const
        {
        	auto nonconst_this = const_cast<member_die *>(this); // HACK: eliminate
        
        	auto enclosing_type_die = boost::dynamic_pointer_cast<type_die>(
            	this->get_parent());
            if (!enclosing_type_die) return boost::optional<Dwarf_Unsigned>();
            
			if (!this->get_data_member_location())
			{
				// if we don't have a location for this field,
				// we tolerate it iff it's the first one in a struct/class
				// OR contained in a union
                // HACK: support class types (and others) here
				if (
				(  (enclosing_type_die->get_tag() == DW_TAG_structure_type ||
					enclosing_type_die->get_tag() == DW_TAG_class_type)
                 && /*static_cast<abstract_dieset::position>(*/nonconst_this->iterator_here().base()/*)*/ == 
                 	/*static_cast<abstract_dieset::position>(*/
                    	boost::dynamic_pointer_cast<structure_type_die>(enclosing_type_die)
                 			->member_children_begin().base().base().base()/*)*/
				) || enclosing_type_die->get_tag() == DW_TAG_union_type)
				
				{
					return boost::optional<Dwarf_Unsigned>(0U);
				}
				else 
				{
					// error
					std::cerr << "Warning: encountered DWARF type lacking member locations: "
						<< *enclosing_type_die << std::endl;
					return boost::optional<Dwarf_Unsigned>();
				}
			}
			else if (this->get_data_member_location()->size() != 1)
			{
				// error
				std::cerr << "Warning: encountered DWARF type with member locations I didn't understand: "
					<< *enclosing_type_die << std::endl;
				return boost::optional<Dwarf_Unsigned>();
			}
			else
			{
				return dwarf::lib::evaluator(
					this->get_data_member_location()->at(0), 
					this->get_ds().get_spec(),
					std::stack<Dwarf_Unsigned>(std::deque<Dwarf_Unsigned>(1, 0UL))).tos();
			}
        }
/* from spec::inheritance_die */
		boost::optional<Dwarf_Unsigned> 
        inheritance_die::byte_offset_in_enclosing_type() const
        {
        	// FIXME
            return boost::optional<Dwarf_Unsigned>();
        }
    }
    namespace lib
    {
/* from lib::basic_die */
        basic_die::basic_die(dieset& ds, boost::shared_ptr<lib::die> p_d)
         : p_d(p_d), p_ds(&ds)
        {
        	/*if (p_d)
            {
	        	Dwarf_Half ret; p_d->tag(&ret);
    	        if (ret == DW_TAG_compile_unit) m_parent_offset = 0UL;
                else m_parent_offset = p_ds->find_parent_offset_of(this->get_offset());
            }
            else*/ m_parent_offset = 0UL;
        }

		Dwarf_Off 
        basic_die::get_offset() const
        {
        	Dwarf_Off ret; p_d->offset(&ret); return ret;
        }
        
        Dwarf_Half 
        basic_die::get_tag() const
        {
        	Dwarf_Half ret; p_d->tag(&ret); return ret;
        }
        
        boost::shared_ptr<spec::basic_die> 
        basic_die::get_parent() 
        {
        	if (this->get_offset() == 0UL) throw No_entry();
            assert(p_d);
	        Dwarf_Half ret; p_d->tag(&ret);
    	    if (ret == DW_TAG_compile_unit) m_parent_offset = 0UL;
            else m_parent_offset = p_ds->find_parent_offset_of(this->get_offset());
            if (m_parent_offset == 0UL) return p_ds->toplevel();
        	else return p_ds->get(m_parent_offset);
        }

        boost::shared_ptr<spec::basic_die> 
        basic_die::get_first_child() 
        {
            boost::shared_ptr<die> p_tmp = boost::make_shared<die>(*p_d);
            return p_ds->get(p_tmp);
        }

        boost::shared_ptr<spec::basic_die> 
        basic_die::get_next_sibling() 
        {
            boost::shared_ptr<die> p_tmp = boost::make_shared<die>(*p_ds->p_f, *p_d);
            return p_ds->get(p_tmp);
        }
        
        Dwarf_Off basic_die::get_first_child_offset() const
        //{ Dwarf_Off off; boost::make_shared<die>(*p_d)->offset(&off); return off; }
        { return const_cast<basic_die *>(this)->get_first_child()->get_offset(); }

        Dwarf_Off basic_die::get_next_sibling_offset() const
        //{ Dwarf_Off off; boost::make_shared<die>(*p_ds->p_f, *p_d)->offset(&off); return off; }
        { return const_cast<basic_die *>(this)->get_next_sibling()->get_offset(); }
       
        boost::optional<std::string> 
        basic_die::get_name() const
        {
        	char *ret;
        	switch(p_d->name(&ret))
            {
                case DW_DLV_OK: return std::string(ret);
                default: return 0;
            }
        }
        
        const spec::abstract_def& 
        basic_die::get_spec() const
        {
        	return spec::dwarf3; // FIXME
        }

        const abstract_dieset& basic_die::get_ds() const
        {
        	return *p_ds;
        }
        abstract_dieset& basic_die::get_ds()
        {
        	return *p_ds;
        }
        
        std::map<Dwarf_Half, encap::attribute_value> basic_die::get_attrs() 
        {
        	std::map<Dwarf_Half, encap::attribute_value> ret;
            if (this->p_d)
            {
			    attribute_array arr(*this->p_d);
                int retval;
			    for (int i = 0; i < arr.count(); i++)
			    {
				    Dwarf_Half attr; 
				    dwarf::lib::attribute a = arr.get(i);
				    retval = a.whatattr(&attr);
				    ret.insert(std::make_pair(attr, encap::attribute_value(this->get_ds(), a)));
			    } // end for
            }
            return ret;
        }

/* from lib::compile_unit_die */
/*		Dwarf_Unsigned 
        compile_unit_die::get_language() const
        {
        	Dwarf_Bool ret;
            assert((p_d->hasattr(DW_AT_language, &ret), ret));
            return DW_LANG_C; // FIXME
        }*/

/* from lib::subprogram_die */
/*       	boost::optional<boost::shared_ptr<spec::basic_die> > 
        subprogram_die::get_type() const
        {
        	Dwarf_Off type_off;
            lib::attribute_array arr(*p_d);
            lib::attribute attr = arr[DW_AT_type];
            encap::attribute_value val(*p_ds, attr);
            type_off = val.get_ref().off;
        	return (*p_ds)[type_off];
        }*/

/* from lib::dieset */
//         std::deque< abstract_dieset::position >
//         dieset::path_from_root(Dwarf_Off off)
//         {
//         	/* This is an iterative, depth-first walk of a tree with no
//              * up-links. 
//              * We keep a list of our current path from the root. */
//         	std::deque< position > current_path;
//             
//             /* We can't use the factory in this function because factory-constructed
//              * classes try to recover their parent in the constructur, which calls
//              * this method and sets up an infinite loop. */
//             boost::shared_ptr<lib::die> first_child = boost::make_shared<die>(*p_f);
//             Dwarf_Off first_child_offset; first_child->offset(&first_child_offset);
// 	            //toplevel()->get_first_child();
//             //boost::make_shared<die>(*p_f);
//             abstract_dieset::position tmp = {this, first_child_offset};
//             current_path.push_back(tmp);
//             
// 			boost::shared_ptr<lib::die> current = first_child;
//             while (true)
//             {
//                 /* Keep on trying to move down */
//                 while (true)
//                 {
//                     boost::shared_ptr<lib::die> lower;
//                     boost::shared_ptr<lib::die> next;
//                     try
//                     {
//                 	    //lower = boost::make_shared<die>(*current);
//                         //lower = current->get_first_child();
//                         // *** test begins here
//                         //Dwarf_Off lower_offset;
//                         //lower->offset(&lower_offset);
//                         Dwarf_Off lower_offset = current->get_first_child_offset();
//                        	abstract_dieset::position tmp = {this, lower_offset};
// 						current_path.push_back(tmp);
//                         
//                         if (lower_offset == off) 
//                         {
// 	                    	return current_path;
//                         }
//                         // *** test ends here
//                         current = current->get_first_child();
//                     }
//                     catch (lib::No_entry)
//                     {
//                 	    /* If that doesn't work, try moving to the next sibling 
//                          * If *that* doesn't work, retreat up a level and try again.
//                          * We terminate if we've retreated up to the top.
//                          * Retreating up to the top
//                          */
//                         do
//                         {
//                             try
//                             {
// 	                            //boost::make_shared<die>(*p_f, *current);
//                                 //next = current->get_next_sibling();
//                                 Dwarf_Off next_offset = current->get_next_sibling_offset();
// 			                    abstract_dieset::position tmp = {this, next_offset};
//                                 current_path.push_back(tmp);
//                                 if (next_offset == off)
//                                 {
//                                     return current_path;
//                                 }
//                                 break;
//                             }
//                             catch (lib::No_entry) 
//                             {
//                             	if (current_path.size() == 0)
//                                 {
//                                 	assert(false); // didn't find the parent!
//                                 }
//                             	//current = current_path.at(current_path.size() - 1);
//                                 //current = boost::make_shared<die>(*p_f, 
//                                 //	current_path.at(current_path.size() - 1).off);
//                                 current = current->get_next_sibling();
//                                 current_path.pop_back();
//                             } // continue
// 	                    } while (true);
//                     }
//                 }
//             }
//         }
        std::deque< spec::abstract_dieset::position > dieset::path_from_root(Dwarf_Off off)
        {
//             std::deque< spec::abstract_dieset::position > /*reverse_*/path;
//             auto cur = (*this)[off];
//             spec::abstract_dieset::position tmp = {this, cur->get_offset()};
// 			/*reverse_*/path.push_back(tmp);
//             while (cur->get_offset() != 0UL)
//             {
//                 cur = cur->get_parent();
//    		        spec::abstract_dieset::position tmp = {this, cur->get_offset()};
//                 /*reverse_*/path./*push_back*/push_front(tmp);
//             }
//             //std::reverse(reverse_path.begin(), reverse_path.end());
//             return /*reverse_*/path;
			return this->find(off).base().path_from_root;
        }

	}
    namespace spec
    {
        abstract_dieset::basic_iterator_base::basic_iterator_base(
        	abstract_dieset& ds, Dwarf_Off off,
            const std::deque<position>& path_from_root,
            policy& pol)
        : position({&ds, off}), 
          path_from_root(path_from_root),
          m_policy(pol) 
        { 
        	/* We've been given an offset but no path. So search for the 
             * offset, which will give us the path. */
        	if (off != 0UL && 
            	off != std::numeric_limits<Dwarf_Off>::max() &&
                path_from_root.size() == 0) this->path_from_root = ds.find(off).path();
            canonicalize_position(); 
        }
		
		/* Partial order on iterators -- these are comparable when they share a parent. */
		bool
		abstract_dieset::iterator::shares_parent_pos(const abstract_dieset::iterator& i) const
		{ return this->base().p_ds == i.base().p_ds
			&& 
			this->dereference()->get_parent()->get_offset() 
			== i.dereference()->get_parent()->get_offset();
		}
		bool
		abstract_dieset::iterator::operator<(const iterator& i) const
		{ return this->shares_parent_pos(i) && 
		  this->dereference()->get_offset() < i.dereference()->get_offset(); 
		}

        bool 
		file_toplevel_die::is_visible::operator()(
			boost::shared_ptr<spec::basic_die> p) const
        {
            auto p_el = boost::dynamic_pointer_cast<program_element_die>(p);
            if (!p_el) return true;
            return !p_el->get_visibility() 
                || *p_el->get_visibility() != DW_VIS_local;
        }
//                 bool operator()(Die_encap_base& d) const
//                 {
//                     try
//                     {
//                         Die_encap_is_program_element& el = 
//                             dynamic_cast<Die_encap_is_program_element&>(d);
//                         return !el.get_visibility() 
//                             || *el.get_visibility() != DW_VIS_local;
//                     } catch (std::bad_cast e) { return true; }
//                 }

        boost::shared_ptr<basic_die>
        file_toplevel_die::visible_named_child(const std::string& name)
        { 
            is_visible visible;
            for (auto i_cu = compile_unit_children_begin();
                    i_cu != compile_unit_children_end(); i_cu++)
            {
                //std::cerr << "Looking for child named " << name << std::endl;
                for (auto i = (*i_cu)->children_begin();
                        i != (*i_cu)->children_end();
                        i++)
                {
                    if (!(*i)->get_name()) continue;
                    //std::cerr << "Testing candidate die at offset " << (*i)->get_offset() << std::endl;
                    if ((*i)->get_name() 
                        && *((*i)->get_name()) == name
                        && visible(*i))
                    { 
                        return *i;
                    }
                }
            }
            return boost::shared_ptr<basic_die>();
        }            
    }
    namespace lib
    {
		/* WARNING: only use find() when you're not sure that a DIE exists
		 * at offset off. Otherwise use operator[]. */
		abstract_dieset::iterator 
		dieset::find(Dwarf_Off off)  
		{
			//abstract_dieset::iterator i = this->begin();
			//while (i != this->end() && i.pos().off != off) i++;
			//return i;
			
			/* We can do better than linear search, using the properties of
			 * DWARF offsets.  */
			boost::shared_ptr<spec::basic_die> current = this->toplevel();
			std::deque<position> path_from_root(1, (position){this, 0UL}); // start with root node only
			/* We do this because iterators can point at the root, and all iterators should 
			 * have the property that the last element in the path is their current node. */
			 
			while (current->get_offset() != off)
			{
				// we haven't reached our target yet, so walk through siblings...
				auto child = current->get_first_child(); // index for loop below
				/* Note that we may throw No_entry here ^^^ -- this happens
				 * if and only if our search has failed, which is what we want. */
				boost::shared_ptr<spec::basic_die> prev_child; // starts at 0
				
				// ...linear search for the child we should accept or descend to
				// (NOTE: if they first child is the one we want, we'll go round once,
				// until that is now prev_child and child is the next (if any)
				while (child && !(child->get_offset() > off))
				{
					// keep going
					prev_child = child;
					try
					{
						child = child->get_next_sibling();
					} 
					catch (No_entry)
					{
						// reached the last sibling
						child = boost::shared_ptr<spec::basic_die>();
					}
				}
				// on terminating this loop: child is *after* the one we want, or null
				// prev_child is either equal to it, or is an ancestor of it
				assert(prev_child && (!child || child->get_offset() > off)
					&& prev_child->get_offset() <= off);
					
				current = prev_child; // we either terminate on this child, or descend under it
				// ... either way, remember its position
				// (sanity check: make sure we're not doubling up a path entry)
				assert(path_from_root.size() != 0 &&
					(path_from_root.back() != (position){this, prev_child->get_offset()}));
				path_from_root.push_back((position){this, prev_child->get_offset()});
			}
			
			return abstract_dieset::iterator((position){this, off}, path_from_root);
		}
        
        abstract_dieset::iterator 
        dieset::begin() 
        {
    		return abstract_dieset::iterator(*this, 0UL, 
            	std::deque<position>(1, (position){this, 0UL}));
        }
        
        abstract_dieset::iterator 
        dieset::end() 
        {
	        return abstract_dieset::iterator(*this, 
            	std::numeric_limits<Dwarf_Off>::max(),
                std::deque<position>());
		} 
        
		Dwarf_Off 
		dieset::find_parent_offset_of(Dwarf_Off off)
		{
			if (off == 0UL) throw No_entry();
			//else if (
			if (this->parent_cache.find(off) != this->parent_cache.end())
			{
				return this->parent_cache[off];
			}
			
			// NOTE: we use find() so that we get the path not just die ptr
			auto path = this->find(off).path();
			assert(path.back().off == off);
			if (path.size() > 1) 
			{ 
				position parent_pos = *(path.end() - 2);
				this->parent_cache[off] = parent_pos.off; // our answer is the *new* last element
				return parent_pos.off;
			}
			else throw lib::No_entry();
		} 

        boost::shared_ptr<basic_die> 
        dieset::find_parent_of(Dwarf_Off off)
        {
        	return get(find_parent_offset_of(off));
        }

        boost::shared_ptr<lib::basic_die> 
        dieset::get(Dwarf_Off off)
        {
        	if (off == 0UL) return boost::dynamic_pointer_cast<lib::basic_die>(toplevel());
            else
            {
            	boost::shared_ptr<die> p_tmp = boost::make_shared<die>(*p_f, off);
	            return get(p_tmp);
            }
        }
        
        boost::shared_ptr<lib::basic_die>
        dieset::get(boost::shared_ptr<die> p_d)
        {
        	/* Given a raw die, we make, and return, a basic_die
             * (offering our ADT interface)
             * or one of its children. */
            Dwarf_Half tag;
            Dwarf_Off off;
            int ret = p_d->offset(&off); assert(ret == DW_DLV_OK);
            switch(p_d->tag(&tag), tag)
            {
#define factory_case(name, ...) \
case DW_TAG_ ## name: return boost::make_shared<lib:: name ## _die >(*this, p_d);
#include "dwarf3-factory.h"
#undef factory_case
                default: return boost::make_shared<lib::basic_die>(*this, p_d);
            }
        }
        
        boost::shared_ptr<spec::basic_die> dieset::operator[](Dwarf_Off off) const
        {
        	if (off == 0UL) return boost::shared_ptr<spec::basic_die>(m_toplevel);
            return const_cast<dieset *>(this)->get(off);
        }
		
        boost::shared_ptr<spec::file_toplevel_die> dieset::toplevel()
        {
        	return boost::dynamic_pointer_cast<spec::file_toplevel_die>(m_toplevel);
        }
        
//         encap::rangelist dieset::rangelist_at(Dwarf_Unsigned i) const
//         {
//         	return encap::rangelist(this->p_f->get_ranges(), i);
//         }
    }
    namespace spec
    {
        //abstract_dieset::children_policy::children_policy(
        
        abstract_dieset::default_policy abstract_dieset::default_policy_sg;
        // depth-first traversal
        int abstract_dieset::default_policy::increment(position& pos,
        	std::deque<abstract_dieset::position>& path)
        {
#define print_path std::cerr << "Path is now: "; for (unsigned i = 0; i < path.size(); i++) std::cerr << ((i > 0) ? ", " : "") << path.at(i).off; std::cerr << std::endl
            assert(path.size() == 0 || path.back().off == pos.off);
            // if we have children, descend there...
            try
            {
            	pos.off = (*pos.p_ds)[pos.off]->get_first_child_offset();//->get_offset();
                	//boost::make_shared<lib::die>(*(*pos.p_ds)[pos.off]->p_d)->offset(&pos.off);
                //std::cerr << "Descending from offset " << ((path.size() > 0) ? path.back().off : 0UL) << " to child " << pos.off << std::endl;
                path.push_back((position){pos.p_ds, pos.off});
                //print_path;
               return 0;
            }
            catch (No_entry) {}

            //print_path; std::cerr << "Offset " << ((path.size() > 0) ? path.back().off : 0UL) << " has no children, trying siblings." << std::endl;
            
            // else look for later siblings right here
            int number_of_pops = 1;
            //if (path.size() == 0) mutable_path = pos.p_ds->path_from_root(pos.off);
            while (path.size() > 0)
            {
            	assert(path.size() == 0 || path.back().off == pos.off);
                try
                {
                	//Dwarf_Off tmp = (*pos.p_ds)[pos.off]->get_next_sibling_offset();
                    //print_path; std::cerr << "Found next sibling of " << pos.off << " is " << tmp << ", following" << std::endl;
            	    pos.off = (*pos.p_ds)[pos.off]->get_next_sibling_offset();//->get_offset();
                    	//boost::make_shared<lib::die>(pos.p_ds->p_f, *(*pos.p_ds)[pos.off]->p_d)->offset(&pos.off);
            		path.pop_back(); // our siblings have a different last hop
                    path.push_back(pos);
                    //print_path;
                    return number_of_pops;
                }
                catch (No_entry) 
                {
                    // else retreat higher up
                    //std::cerr << "Offset " << pos.off << " has no subsequent siblings, retreating." << std::endl;;
                    path.pop_back();
                    //print_path;
                    if (path.size() > 0)
                    {
	                    //std::cerr << " Will next try siblings of " << path.back().off << std::endl;
                    	pos.off = path.back().off;
                    }
                    //else  std::cerr << " Will terminate." << std::endl;
			    }
            }
            // if we got here, there is nothing left in the tree...
            assert(path.size() == 0);
            // ... so set us to the end sentinel
            pos.off = std::numeric_limits<Dwarf_Off>::max();
            path = std::deque<position>();
            return path.size();
        }
        int abstract_dieset::default_policy::decrement(position& pos,
        	std::deque<abstract_dieset::position>& path)
        {
        	assert(false);
        }

        // breadth-first traversal
        int abstract_dieset::bfs_policy::increment(position& pos,
        	std::deque<abstract_dieset::position>& path)
        {
        	int retval;
        	/* Try to explore siblings, remembering children. */
            try
            {
            	try
                {
                	Dwarf_Off first_child_off = (*pos.p_ds)[pos.off]->get_first_child_offset();
                    auto queued_path = path; 
                    queued_path.push_back((position){pos.p_ds, first_child_off});
                    this->m_queue.push_back(queued_path);
                } 
                catch (No_entry)
                {
                    /* No children -- that's okay, just carry on and don't enqueue anything. */
                }
                /* Now find that sibling. */
            	pos.off = (*pos.p_ds)[pos.off]->get_next_sibling_offset();
                // if that succeeded, we have a next node -- calculate our new path
                path.pop_back();
                path.push_back((position){pos.p_ds, pos.off});
                retval = 0;
            } catch (No_entry)
            {
            	/* No more siblings, so take from the queue. */
                if (this->m_queue.size() > 0)
                {
					pos = this->m_queue.front().back(); 
                    // retval is the number of levels up the tree we're moving
                    retval = path.size() - this->m_queue.front().size();
                    // finished processing head queue element
                    this->m_queue.pop_front();
                }
                else
                {
                	/* That's it! Use end sentinel. */
                    pos.off = std::numeric_limits<Dwarf_Off>::max();
                    path = std::deque<position>();
                    retval = 0;
                }
            }
            return retval;
        }
        int abstract_dieset::bfs_policy::decrement(position& pos,
        	std::deque<abstract_dieset::position>& path) 
        {
        	assert(false);
        }
        
        // siblings-only traversal
        abstract_dieset::siblings_policy abstract_dieset::siblings_policy_sg;
        int abstract_dieset::siblings_policy::increment(position& pos,
        	std::deque<abstract_dieset::position>& path)
        {
        	auto maybe_next_off = (*pos.p_ds)[pos.off]->next_sibling_offset();
        	if (maybe_next_off)
            {
                pos.off = *maybe_next_off;
                path.pop_back(); path.push_back((position){pos.p_ds, *maybe_next_off});
            }
            else 
            {
                /* That's it! Use end sentinel. */
                pos.off = std::numeric_limits<Dwarf_Off>::max();
                path = std::deque<position>();
            }
        	return 0;
        }
        int abstract_dieset::siblings_policy::decrement(position& pos,
        	std::deque<abstract_dieset::position>& path) 
        {
        	assert(false);
        }
    }
    namespace lib
    {

		boost::shared_ptr<spec::basic_die> file_toplevel_die::get_first_child()
		{
			// We have to explicitly loop through CU headers, 
    		// to set the CU context when getting dies.
			Dwarf_Unsigned cu_header_length;
			Dwarf_Half version_stamp;
			Dwarf_Unsigned abbrev_offset;
			Dwarf_Half address_size;
			Dwarf_Unsigned next_cu_header;

			int retval;
			p_ds->p_f->reset_cu_context();

			retval = p_ds->p_f->next_cu_header(&cu_header_length, &version_stamp,
				&abbrev_offset, &address_size, &next_cu_header);
			if (retval != DW_DLV_OK)
			{
				std::cerr << "Warning: couldn't get first CU header! no debug information imported" << std::endl;
        		throw No_entry();
			}

    		switch (version_stamp)
    		{
        		case 2: p_spec = &dwarf::spec::dwarf3; break;
        		default: throw std::string("Unsupported DWARF version stamp!");
    		}
    		prev_version_stamp = version_stamp;

    		return boost::make_shared<compile_unit_die>(*p_ds, 
        		boost::make_shared<dwarf::lib::die>(*p_ds->p_f));
		}

		Dwarf_Off file_toplevel_die::get_first_child_offset() const
		{
			return const_cast<file_toplevel_die*>(this)->get_first_child()->get_offset();
		}
		Dwarf_Off file_toplevel_die::get_next_sibling_offset() const
		{
			throw No_entry();
		}
		
		boost::shared_ptr<spec::basic_die> compile_unit_die::get_next_sibling()
		{
			int retval;
			Dwarf_Half prev_version_stamp 
				= boost::dynamic_pointer_cast<file_toplevel_die>(p_ds->toplevel())
					->prev_version_stamp;
			Dwarf_Half version_stamp = prev_version_stamp;
			Dwarf_Off next_cu_offset;

			// first reset the CU context (pesky stateful API)
			retval = p_ds->p_f->reset_cu_context();
			if (retval != DW_DLV_OK) // then it must be DW_DLV_ERROR
			{
				assert(retval == DW_DLV_ERROR);
				std::cerr << "Warning: couldn't get first CU header! no debug information imported" << std::endl;
				throw No_entry();
			}

			// now find us
			for (retval = p_ds->p_f->next_cu_header(/*&cu_header_length*/0, &version_stamp,
					/*&abbrev_offset*/0, /*&address_size*/0, &next_cu_offset);
					retval != DW_DLV_NO_ENTRY; // termination condition (negated)
					retval = p_ds->p_f->next_cu_header(/*&cu_header_length*/0, &version_stamp,
									/*&abbrev_offset*/0, /*&address_size*/0, &next_cu_offset))
			{
				// only support like-versioned CUs for now
				assert(prev_version_stamp == version_stamp);
				// now we can access the CU by constructing a lib::die under the current CU state
				auto p_cu = boost::make_shared<die>(*p_ds->p_f);
				Dwarf_Off off;
				if ((p_cu->offset(&off), off) == this->get_offset()) // found us
				{
					break;
				}
			}

			if (retval == DW_DLV_NO_ENTRY) // failed to find ourselves!
			{
				assert(false);
			}

			// now go one further
			retval = p_ds->p_f->next_cu_header(/*&cu_header_length*/0, &version_stamp,
					/*&abbrev_offset*/0, /*&address_size*/0, &next_cu_offset);
			if (retval == DW_DLV_NO_ENTRY) throw No_entry();

			return boost::make_shared<compile_unit_die>(*p_ds, 
			boost::make_shared<dwarf::lib::die>(*p_ds->p_f));
		} // end get_next_sibling()
	} // end namespace lib
} // end namespace dwarf
